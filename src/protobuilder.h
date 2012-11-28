#include <vector>

#define THROW_BAD_ARGS(FAIL_MSG) return ThrowException(Exception::TypeError(String::New(FAIL_MSG)));
#define THROW_ERROR(FAIL_MSG) return ThrowException(Exception::Error(String::New(FAIL_MSG)));
#define CHECK_N_ARGS(MIN_ARGS) if (args.Length() < MIN_ARGS) THROW_BAD_ARGS("Expected " #MIN_ARGS " arguments")

const PropertyAttribute CONST_PROP = static_cast<PropertyAttribute>(ReadOnly|DontDelete);

class ProtoBuilder{
public:
	typedef void (*InitFn)(Handle<Object> target);
	typedef std::vector<InitFn> InitFnList;
	static InitFnList initFns;

	static void initAll(Handle<Object> target){
		for (InitFnList::iterator it=initFns.begin() ; it < initFns.end(); it++ ){
			(**it)(target);
		}
	}

	ProtoBuilder(const char *_name, ProtoBuilder::InitFn initfn=NULL):name(_name){
		if (initfn) ProtoBuilder::initFns.push_back(initfn);
	}

	void init(InvocationCallback constructor){
		tpl = Persistent<FunctionTemplate>::New(FunctionTemplate::New(constructor));
		tpl->InstanceTemplate()->SetInternalFieldCount(1);
		tpl->SetClassName(String::NewSymbol(name));
	}

	void inherit(ProtoBuilder& other){
		tpl->Inherit(other.tpl);
	}

	Handle<Object> get(){
		return tpl->GetFunction();	
	}

	void addToModule(Handle<Object> target){
		target->Set(String::NewSymbol(name), get());
	}

	void addMethod(const char* name, v8::InvocationCallback fn){
		NODE_SET_PROTOTYPE_METHOD(tpl, name, fn);
	}

	void addStaticMethod(const char* name, v8::InvocationCallback fn){
		NODE_SET_METHOD(tpl, name, fn);
	}

	void addAccessor(const char* name, AccessorGetter getter, AccessorSetter setter=0){
		tpl->InstanceTemplate()->SetAccessor(String::NewSymbol(name), getter, setter);
	}

	Persistent<FunctionTemplate> tpl;
	const char* const name;
};

template <class T>
class Proto : public ProtoBuilder{
public:
	typedef T wrappedType;

	Proto(const char *_name, ProtoBuilder::InitFn initfn=NULL):
		ProtoBuilder(_name, initfn){}

	Handle<Value> create(T* v, Handle<Value> arg1 = Undefined(), Handle<Value> arg2 = Undefined()){
		if (!v) return Undefined();
		Handle<Value> args[3] = {External::New(v), arg1, arg2};
		Handle<Object> o = tpl->GetFunction()->NewInstance(3, args);
		return o;
	}

	void wrap(Handle<Object> obj, T* v){
		obj->SetPointerInInternalField(0, v);
	}

	T* unwrap(Handle<Value> handle){
		if (!handle->IsObject()) return NULL;
		Handle<Object> o = Handle<Object>::Cast(handle);
		if (o->InternalFieldCount() == 0) return NULL;
		if (o->FindInstanceInPrototypeChain(tpl).IsEmpty()) return NULL;
		return static_cast<T*>(o->GetPointerFromInternalField(0));
	}
};

inline static void setConst(Handle<Object> obj, const char* const name, Handle<Value> value){
	obj->Set(String::NewSymbol(name), value, CONST_PROP);
}

#define ENTER_CONSTRUCTOR(MIN_ARGS) \
	HandleScope scope;              \
	if (!args.IsConstructCall()) THROW_ERROR("Must be called with `new`!"); \
	CHECK_N_ARGS(MIN_ARGS);

#define ENTER_CONSTRUCTOR_POINTER(PROTO, MIN_ARGS) \
	ENTER_CONSTRUCTOR(MIN_ARGS)             \
	if (!args.Length() || !args[0]->IsExternal()){ \
		THROW_BAD_ARGS("This type cannot be created directly!"); \
	}else{ \
		args.This()->SetInternalField(0, args[0]); \
	} \
	auto self = PROTO.unwrap(args.This()); \
	(void) self \

#define ENTER_METHOD(PROTO, MIN_ARGS) \
	HandleScope scope;                \
	CHECK_N_ARGS(MIN_ARGS);           \
	auto self = PROTO.unwrap(args.This()); \
	(void) self

#define ENTER_ACCESSOR(PROTO) \
		HandleScope scope;                \
		auto self = PROTO.unwrap(info.Holder());

#define UNWRAP_ARG(PROTO, NAME, ARGNO)     \
	auto NAME = PROTO.unwrap(args[ARGNO]); \
	if (!NAME)                             \
		THROW_BAD_ARGS("Parameter " #NAME " (" #ARGNO ") is of incorrect type");

#define STRING_ARG(NAME, N) \
	std::string NAME; \
	if (args.Length() > N-1){ \
		if (!args[N]->IsString()) \
			THROW_BAD_ARGS("Parameter " #NAME " (" #N ") should be string"); \
		NAME = *String::Utf8Value(args[N]->ToString()); \
	}

#define DOUBLE_ARG(NAME, N) \
	if (!args[N]->IsNumber()) \
		THROW_BAD_ARGS("Parameter " #NAME " (" #N ") should be number"); \
	double NAME = args[N]->ToNumber()->Value();

#define INT_ARG(NAME, N) \
	if (!args[N]->IsNumber()) \
		THROW_BAD_ARGS("Parameter " #NAME " (" #N ") should be number"); \
	int NAME = args[N]->ToInt32()->Value();

#define ARRAY_UNWRAP_ARG(PROTO, TP, NAME, ARGNO)                                \
	if (!args[ARGNO]->IsArray())                                             \
		THROW_BAD_ARGS("Parameter " #NAME " (" #ARGNO ") should be array"); \
	std::vector<TP*> NAME;                         \
	Local<Array> NAME##_arg = Local<Array>::Cast(args[ARGNO]);              \
	for (int NAME##_i=0 ; NAME##_i < (int) NAME##_arg->Length() ; ++NAME##_i) {   \
		auto elem = PROTO.unwrap(NAME##_arg->Get(NAME##_i)); \
		if (!elem) THROW_BAD_ARGS("Parameter " #NAME " (" #ARGNO ") contains element of invalid type"); \
		NAME.push_back(elem);                                           \
	} 
