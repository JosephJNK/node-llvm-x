llvm = module.exports = require('bindings')('llvm')

llvm.initializeNativeTarget()

var ffi
try{
	ffi = require('ffi')
}catch (e){
	ffi = null
}

llvm.Function.prototype.addBasicBlock = function (nameOrBlock){
	if (typeof nameOrBlock == 'string'){
		nameOrBlock = new llvm.BasicBlock(this.context, nameOrBlock);
	}
	return this._addBasicBlock(nameOrBlock);
}

llvm.ExecutionEngine.prototype.getFFIFunction = function(func){
	if (!ffi) throw new Error("Couldn't load FFI module")
	var ptr = this.getPointerToFunction(func);

	// TODO: generate types from func.functionType
	var fn = ffi.ForeignFunction(ptr, 'double', ['double'])
	fn.executionEngine = this; // keep it from being GC'd
	return fn
}