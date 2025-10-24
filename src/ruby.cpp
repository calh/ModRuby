#include <ruby.h>
#include <ruby/encoding.h>
#include <ruby/thread.h>
#include <sstream>

#include "ruby.hpp"

using std::string;

extern "C" int ruby_thread_has_gvl_p(void);

namespace
{
// Holds the parameters/result for invoking rb_protect under the dispatcher.
struct ProtectCallContext
{
    VALUE (*func)(VALUE);
    VALUE arg;
    int error;
    VALUE result;
};

// Trampoline that re-enters rb_protect while the GVL is held.
static void* protect_with_gvl(void* data)
{
    ProtectCallContext* ctx = static_cast<ProtectCallContext*>(data);
    ctx->error = 0;
    ctx->result = rb_protect(ctx->func, ctx->arg, &ctx->error);

    return NULL;
}

// Calls the supplied Ruby C function inside rb_protect, reacquiring the GVL
// if the current native thread does not already own it.
static VALUE call_with_protect(VALUE (*func)(VALUE), VALUE arg, int* error)
{
    ProtectCallContext ctx { func, arg, 0, Qnil };

    if (ruby_thread_has_gvl_p())
    {
        ctx.error = 0;
        ctx.result = rb_protect(func, arg, &ctx.error);
    }
    else
    {
        rb_thread_call_with_gvl(protect_with_gvl, &ctx);
    }

    if (error != NULL)
    {
        *error = ctx.error;
    }

    return ctx.result;
}

// Context structure for safely calling rb_load_protect from non-Ruby threads.
struct LoadCallContext
{
    const char* filename;
    int anonymous;
    int error;
};

// Executes rb_load_protect while the GVL is held.
static void* load_with_gvl(void* data)
{
    LoadCallContext* ctx = static_cast<LoadCallContext*>(data);
    ctx->error = 0;
    rb_load_protect(rb_str_new_cstr(ctx->filename), ctx->anonymous, &ctx->error);
    return NULL;
}

// Runs the given function while ensuring the GVL is owned by this thread.
static void perform_with_gvl(void* (*func)(void*), void* data)
{
    if (ruby_thread_has_gvl_p())
    {
        func(data);
    }
    else
    {
        rb_thread_call_with_gvl(func, data);
    }
}
} // namespace

namespace ruby
{

Object::Object(const char* name, int n, ...)
    : self(Qnil), _class_name()
{
    va_list vl;
    va_start(vl, n);

    self = ruby::create_object(name, n, vl);

    va_end(vl);

    ruby::register_object(self);

    _class_name = name;
}

Object::~Object()
{
    ruby::free_object(self);
}

const char* Object::class_name()
{
    return _class_name.c_str();
}

VALUE Object::method(const char* name, int n, ...)
{
    VALUE* argv = 0;

    if (n > 0)
    {
        argv = ALLOCA_N(VALUE, n);
        va_list ar;
        va_start(ar, n);

        int i;
        for (i = 0; i < n ; i++)
        {
            argv[i] = va_arg(ar, VALUE);
        }

        va_end(ar);
    }

    Arguments arg;
    arg.recv = self;
    arg.id   = rb_intern(name);
    arg.n    = n;
    arg.argv = argv;

    int error = 0;
    VALUE result = call_with_protect( ruby::method_wrap,
                                      reinterpret_cast<VALUE>(&arg),
                                      &error );

    if (error)
    {
        std::string msg;
        msg = "ruby::Object::method() invoking "
              + _class_name + (std::string)"::"
              + name + (std::string)"()" ;

        Exception e(msg.c_str());
        e.backtrace();

        throw e;
    }

    return result;
}

//------------------------------------------------------------------------------
// Memory Management
//------------------------------------------------------------------------------

Objects* Objects::singleton = 0;
VALUE Objects::objects;

Objects* Objects::instance()
{
    if (singleton == NULL)
    {
        singleton = new Objects();
    }

    return singleton;
}

Objects::Objects()
{
    objects = rb_ary_new();
    rb_gc_register_address(&objects);
}

Objects::~Objects()
{
    rb_gc_unregister_address(&objects);
}

void Objects::register_object(VALUE object)
{
    rb_ary_push(objects, object);
}

void Objects::free_object(VALUE object)
{
    rb_ary_delete(objects, object);
}

void Objects::free_all()
{
    if (singleton != NULL)
    {
        delete singleton;
        singleton = NULL;
    }
}

void register_object(VALUE object)
{
    Objects::instance()->register_object(object);
}

void free_object(VALUE object)
{
    Objects::instance()->free_object(object);
}

void free_all()
{
    Objects::free_all();
}

//------------------------------------------------------------------------------
// VM Routines
//------------------------------------------------------------------------------

static bool running = false;

void startup(const char* script_name)
{
    if (running)
    {
        return;
    }

    int fake_argc = 0;
    char* fake_args[fake_argc];
    char** fake_argv = fake_args;

    ruby_sysinit(&fake_argc, &fake_argv);

    // Initialize Ruby itself
    RUBY_INIT_STACK;
    ruby_init();
    ruby_init_loadpath();

    // This used to be the way to do it (https://bugs.ruby-lang.org/issues/7424):
    //
    // static char* args[] = { "ruby", "/dev/null" };
    // ruby_process_options(2, args);

    // But now I get segfault with following stack trace:
    //
    // #0  0x0000000802c97c4e in rb_make_exception () from /usr/local/lib/libruby24.so.24
    // #1  0x0000000802c97423 in rb_frozen_class_p () from /usr/local/lib/libruby24.so.24
    // #2  0x0000000802c9610d in rb_exc_raise () from /usr/local/lib/libruby24.so.24
    // #3  0x0000000802c9433c in rb_loaderror () from /usr/local/lib/libruby24.so.24
    // #4  0x0000000802c94d1f in rb_sys_warning () from /usr/local/lib/libruby24.so.24
    // #5  0x0000000802d5b392 in rb_load_file_str () from /usr/local/lib/libruby24.so.24
    // #6  0x0000000802d5c43f in ruby_process_options () from /usr/local/lib/libruby24.so.24
    //
    // Which seems to imply that it is trying to load /dev/null as a Ruby
    // module. So I modified it and this is what works now:

    static char* args[] = { "ruby" };
    ruby_process_options(1, args);

    // Load Ruby encodings, otherwise we'll get all kinds of "Unitialized
    // constanct Encoding::UTF-7", etc. everywhere.
    rb_enc_find_index("encdb");

    VALUE gem;
    gem = rb_define_module("Gem");
    rb_const_set(gem, rb_intern("Enable"), Qtrue);
    rb_require("rubygems");

    ruby_script((char*)script_name);

    running = true;
}

void shutdown(int exit_code)
{
    free_all();
    ruby_cleanup(0);

    if (running == false)
    {
        return;
    }

    running = false;
}

//------------------------------------------------------------------------------
// Exceptions
//------------------------------------------------------------------------------

Exception::Exception(const char* msg) : _msg(), _backtrace(), _type()
{
    if (msg != NULL)
    {
        _msg = msg;
    }
}

Exception::Exception(const Exception& e) : _msg(), _backtrace(), _type()
{
    *this = e;
}

Exception::~Exception() throw()
{

}

const Exception& Exception::operator=(const Exception& e)
{
    if (this != &e)
    {
        _msg       = e._msg;
        _backtrace = e._backtrace;
        _type      = e._type;
    }

    return *this;
}

const char* Exception::type() const
{
    return _type.c_str();
}

const char* Exception::what() const throw()
{
    return _msg.c_str();
}

const char* Exception::stackdump() const
{
    return _backtrace.c_str();
}

// Convert a ruby exception into C++.
void Exception::backtrace() throw()
{
    _backtrace.clear();

    // Get the glocal exception object
    VALUE exception_instance = rb_gv_get("$!");

    // Get the exception instance class
    VALUE klass = rb_class_path(CLASS_OF(exception_instance));
    _backtrace += "Type    : " + (std::string)RSTRING_PTR(klass) + (std::string)"\n";

    // Store it for later reference
    _type = (std::string)RSTRING_PTR(klass);

    //> Now generate the error message

    // Store exception class name
    _backtrace += RSTRING_PTR(klass);

    // Message: call $!.to_s() to get string representation
    VALUE message = rb_obj_as_string(exception_instance);
    _backtrace += "Message : " + (std::string)RSTRING_PTR(message) + (std::string)"\n";

    // Backtrace:  p (*(RArray*)ary).as.ary
    if (!NIL_P(exception_instance))
    {
        VALUE ary = rb_funcall(exception_instance, rb_intern("backtrace"), 0);

        int c;
        for (c = 0; c < RARRAY_LEN(ary); c++)
        {
            // gdb: p *((RString*)(*(RArray*)ary).as.ary[0])
            _backtrace += (std::string)"From    : " +
                          (std::string)RSTRING_PTR(RARRAY_PTR(ary)[c]) +
                          (std::string)"\n";
        }
    }
}

//------------------------------------------------------------------------------
// Function Calls
//------------------------------------------------------------------------------

extern "C" {

static int collect_hash_vals(VALUE key, VALUE value, VALUE data);

} // extern "C"

static int collect_hash_vals(VALUE key, VALUE value, VALUE data)
{
    std::map<string, string>* values = (std::map<string, string>*)data;

    std::stringstream strm;

    string key_str;

    switch (TYPE(key))
    {
        case T_STRING:
        {
            strm << StringValuePtr(key);
            break;
        }

        case T_FIXNUM:
        {
            strm << NUM2INT(key);
        }

        default:
        {

        }
    }

    key_str = strm.str();

    strm.str(std::string());

    string value_str;

    switch (TYPE(value))
    {
        case T_STRING:
        {
            strm << StringValuePtr(value);
            break;
        }

        case T_FIXNUM:
        {
            strm << NUM2INT(value);
        }

        default:
        {

        }
    }

    value_str = strm.str();

    (*values)[key_str] = value_str;

    return ST_CONTINUE;
}

typedef int (*iterfn)(...);

bool copy_hash(VALUE hash, std::map<std::string, std::string>& values)
{
    rb_hash_foreach(hash, (iterfn)collect_hash_vals, (VALUE)&values);

    return true;
}

VALUE method_wrap(VALUE arg)
{
    Arguments& a = *reinterpret_cast<Arguments*>(arg);

    return rb_funcall2(a.recv, a.id, a.n, a.argv);
}

// Purpose: Call a ruby function in a safe way. Translate ruby errors into c++
// exceptions.
//
//    VALUE Unsafe() {
//        return rb_funcall(
//            self,
//            rb_intern("test"),
//            1,
//            INT2NUM(42)
//        );
//    }
//
//    VALUE Safe() {
//        return ruby::method(
//            self,
//            rb_intern("test"),
//            1,
//            INT2NUM(42)
//        );
//    }

VALUE method(VALUE recv, ID id, int n, ...)
{
    VALUE* argv = 0;

    if (n > 0)
    {
        argv = ALLOCA_N(VALUE, n);
        va_list ar;
        va_start(ar, n);

        int i;
        for (i = 0; i < n ; i++)
        {
            argv[i] = va_arg(ar, VALUE);
        }

        va_end(ar);
    }

    Arguments arg;
    arg.recv = recv;
    arg.id   = id;
    arg.n    = n;
    arg.argv = argv;

    int error = 0;
    VALUE result = call_with_protect(method_wrap, reinterpret_cast<VALUE>(&arg), &error);

    if (error)
    {
        Exception e;
        e.backtrace();
        throw e;
    }

    return result;
}

VALUE vm_method(VALUE recv, ID id, int n, va_list ar)
{
    VALUE* argv = 0;

    if (n > 0)
    {
        argv = ALLOCA_N(VALUE, n);

        int i;
        for (i = 0; i < n ; i++)
        {
            argv[i] = va_arg(ar, VALUE);
        }
    }

    Arguments arg;
    arg.recv = recv;
    arg.id   = id;
    arg.n    = n;
    arg.argv = argv;

    int error = 0;
    VALUE result = call_with_protect(method_wrap, reinterpret_cast<VALUE>(&arg), &error);

    if (error)
    {
        Exception e;
        e.backtrace();
        throw e;
    }

    return result;
}

//------------------------------------------------------------------------------
// Module and Class Instantiation
//------------------------------------------------------------------------------

void eval(const char* code, const char* filename, int sl, VALUE binding)
{
    const char* fn = filename;

    if (fn == NULL)
    {
        fn = "eval";
    }

    // We use this rather than ruby::eval() because this allows us
    // to associated a file name with the eval code, giving us more
    // informative backtraces. Otherwise, ruby::eval() would simply
    // list the source of every frame as "(eval)".

    method( Qnil, rb_intern("eval"), 4,
            rb_str_new2(code), // code
            binding,           // binding
            rb_str_new2(fn),   // filename
            INT2NUM(sl) );     // source line
}

VALUE require_protect(VALUE arg)
{
    const char* filename = reinterpret_cast<const char*>(arg);
    rb_require(filename);

    return Qnil;
}

bool call_function(const char* method, int n, ...)
{
    VALUE ret;
    va_list ar;

    if (n > 0)
    {
        va_start(ar, n);
    }

    try
    {
        ret = ruby::vm_method(Qnil, rb_intern(method), n, ar);
    }
    catch (const ::ruby::Exception& e)
    {
        // User needs this error on command line, so pipe it to stdout.
        fprintf(stdout, "%s\n", e.what());
        ret = Qfalse;
    }

    if (n > 0)
    {
        va_end(ar);
    }

    return ret;
}

void require(const char* filename)
{
    int error = 0;
    call_with_protect(require_protect, reinterpret_cast<VALUE>(filename), &error);

    if (error)
    {
        std::stringstream strm;

        strm << "error loading " << filename << ".rb";

        Exception e(strm.str().c_str());
        e.backtrace();
        throw e;
    }
}

void load(const char* filename, int anonymous)
{
    LoadCallContext ctx { filename, anonymous, 0 };
    perform_with_gvl(load_with_gvl, &ctx);

    if (ctx.error)
    {
        Exception e;
        e.backtrace();
        throw e;
    }
}

struct NewArguments
{
    const char* class_name;
    int n;
    VALUE* argv;

    NewArguments( const char* cname,
                  int n,
                  VALUE* argv)
        : class_name(cname), n(n), argv(argv)
    {

    }
};

VALUE create_object_protect(VALUE arg)
{
    NewArguments& a = *reinterpret_cast<NewArguments*>(arg);
    VALUE class_name = rb_path2class(a.class_name);
    VALUE self = rb_class_new_instance(a.n, a.argv, class_name);

    return self;
}

VALUE create_object(const char* class_name, int n, va_list ar)
{
    VALUE* argv = 0;

    if (n > 0)
    {
        argv = ALLOCA_N(VALUE, n);

        int i;
        for (i = 0; i < n ; i++)
        {
            argv[i] = va_arg(ar, VALUE);
        }
    }

    NewArguments arg(class_name, 0, 0);

    arg.n    = n;
    arg.argv = argv;

    int error = 0;
    VALUE self = call_with_protect(create_object_protect, reinterpret_cast<VALUE>(&arg), &error);

    if (error)
    {
        std::stringstream strm;
        strm << "Error creating Ruby class '" << class_name << "'";

        Exception e(strm.str().c_str());
        e.backtrace();
        throw e;
    }

    return self;
}

void require_class(VALUE x, VALUE cls)
{
    if (rb_obj_is_instance_of(x, cls) == Qfalse)
    {
        rb_raise( rb_eRuntimeError,
                  "wrong argument type %s (expected %s)",
                  rb_obj_classname(x),
                  rb_class2name(cls)
                );
    }
}

} // end namespace ruby
