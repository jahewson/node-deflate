// Copyright (C) 2011 John Hewson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <node.h>
#include <node_buffer.h>
#include <zlib.h>
#include <string.h>

#include <stdio.h>

using namespace v8;
using namespace node;

////////////////////////////////////////////////////////////////////////////////////////////////////////

static Handle<Value> GetZError(int ret) {
 const char* msg;

 switch (ret) {
 case Z_ERRNO:
   msg = "Z_ERRNO";
   break;
 case Z_STREAM_ERROR:
   msg = "Z_STREAM_ERROR";
   break;
 case Z_DATA_ERROR:
   msg = "Z_DATA_ERROR";
   break;
 case Z_MEM_ERROR:
   msg = "Z_MEM_ERROR";
   break;
 case Z_BUF_ERROR:
   msg = "Z_BUF_ERROR";
   break;
 case Z_VERSION_ERROR:
   msg = "Z_VERSION_ERROR";
   break;
 default:
   msg = "Unknown ZLIB Error";
 }
 
 return ThrowException(Exception::Error(String::New(msg)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

class Deflater : ObjectWrap {

  private:
   z_stream strm;
   char* input;
   char* output;
   int output_length;
   int status;
   bool finished;
   int output_chunk_size;
   
  public:

   Deflater() {
      input = NULL;
      output_length = 0;
      output = NULL;
      finished = false;
   }

   int Init(int level, int windowBits, int memLevel, int strategy, int chunk_size) {
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      output_chunk_size = chunk_size;
      return deflateInit2(&strm, level, Z_DEFLATED, windowBits, memLevel, strategy);
   }
   
   int SetInput(int in_length) {
      strm.avail_in = in_length;
      strm.next_in = (Bytef*) input;
      
      if (output == NULL || output_length == output_chunk_size) {
         strm.avail_out = output_chunk_size;
         output = (char*) malloc(output_chunk_size);
         
         if (output == NULL) {
            return Z_MEM_ERROR;
         }
         
         strm.next_out = (Bytef*) output;
      }
      
      return Z_OK;
   }
   
   void ResetOutput() {
      strm.avail_out = output_chunk_size;
      strm.next_out = (Bytef*) output;
   }
   
   int Deflate() {
      int ret = deflate(&strm, Z_NO_FLUSH);
      status = ret;
      output_length = output_chunk_size - strm.avail_out;
      return ret;
   }
   
   int Finish() {
      strm.avail_in = 0;
      strm.next_in = NULL;

      int ret = deflate(&strm, Z_FINISH);
      status = ret;
      
      output_length = output_chunk_size - strm.avail_out;
      
      if (ret == Z_STREAM_END) {
         deflateEnd(&strm);
         finished = true;
      }
      
      return ret;
   }
   
   bool IsOutputBufferFull() {
      return output_length == output_chunk_size;
   }
   
   bool HasFinished() {
      return finished;
   }
   
   int GetOutputLength() {
      return output_length;
   }
   
   char* GetOutput() {
      return output;
   }
   
   // node.js wrapper //
       
   static void Init(v8::Handle<v8::Object> target) {
      Local<FunctionTemplate> t = FunctionTemplate::New(New);

      t->InstanceTemplate()->SetInternalFieldCount(1);
      t->SetClassName(String::NewSymbol("Deflater"));

      NODE_SET_PROTOTYPE_METHOD(t, "write", SetInput);
      NODE_SET_PROTOTYPE_METHOD(t, "deflate", Deflate);
      NODE_SET_PROTOTYPE_METHOD(t, "read", GetOutput);
      NODE_SET_PROTOTYPE_METHOD(t, "flush", Finish);
      
      target->Set(String::NewSymbol("Deflater"), t->GetFunction());
   }
    
   static Handle<Value> New (const Arguments& args) {
      HandleScope scope;
      
      int level = Z_DEFAULT_COMPRESSION;
      int windowBits = 16 + MAX_WBITS; // gzip
      int memLevel = 8;
      int strategy = Z_DEFAULT_STRATEGY;
      int output_chunk_size = 131072; // 128K
      
      int idx = 0;

      if (args.Length() > 0) {
         if(args[idx+0]->IsNumber()) {
            level = args[idx+0]->Int32Value();        
            if (level < 0 || level > 9) {
               Local<Value> exception = Exception::Error(String::New("level must be between 0 and 9"));
               return ThrowException(exception);
            }
         }
         else if (args[idx+0]->IsString()) {
            char* strLevel = *String::AsciiValue(args[idx+0]->ToString());

            if (strcmp(strLevel, "gzip") == 0) {
               windowBits = 16 + MAX_WBITS;
            }
            else if (strcmp(strLevel, "zlib") == 0) {
               windowBits = MAX_WBITS;
            }
            else if (strcmp(strLevel, "deflate") == 0) {
               windowBits = -MAX_WBITS;
            }
            else {
               Local<Value> exception = Exception::TypeError(String::New("bad deflate kind"));
               return ThrowException(exception);
            }
         }
         else if (args[idx+0]->IsUndefined()) {
         }
         else {
            Local<Value> exception = Exception::TypeError(String::New("expected a Number or String"));
            return ThrowException(exception);
         }

         if (args.Length() > 1) {
            if(args[idx+1]->IsNumber()) {
               level = args[idx+1]->Int32Value();        
               if (level < 0 || level > 9) {
                  Local<Value> exception = Exception::Error(String::New("level must be between 0 and 9"));
                  return ThrowException(exception);
               }
            }
            else if (args[idx+1]->IsUndefined()) {
            }
            else {
               Local<Value> exception = Exception::TypeError(String::New("expected a Number"));
               return ThrowException(exception);
            }
         }

         if (args.Length() > 2) {
            if(args[idx+2]->IsNumber()) {
               output_chunk_size = args[idx+2]->Int32Value();        
               if (output_chunk_size < 0) {
                  Local<Value> exception = Exception::Error(String::New("invalid buffer size"));
                  return ThrowException(exception);
               }
            }
            else if (args[idx+2]->IsUndefined()) {
            }
            else {
               Local<Value> exception = Exception::TypeError(String::New("buffer size must be a Number"));
               return ThrowException(exception);
            }
         }
      }
      
      Deflater *deflater = new Deflater();
      deflater->Wrap(args.This());

      int r = deflater->Init(level, windowBits, memLevel, strategy, output_chunk_size);
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return args.This();
   }
   
   static Handle<Value> SetInput(const Arguments& args) {
      Deflater *deflater = ObjectWrap::Unwrap<Deflater>(args.This());
      HandleScope scope;
      
      Local<Object> in = args[0]->ToObject();
      ssize_t length = Buffer::Length(in);

      // copy the input buffer, because it can be kept for several deflate() calls
      deflater->input = (char*)realloc(deflater->input, length);
      
      if (deflater->input == NULL) {
          return GetZError(Z_MEM_ERROR);
      }
      
      memcpy(deflater->input, Buffer::Data(in), length);

      int r = deflater->SetInput(length);
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return scope.Close(Undefined());
   }
   
   static Handle<Value> Deflate(const Arguments& args) {
      Deflater *deflater = ObjectWrap::Unwrap<Deflater>(args.This());
      HandleScope scope;

      int r = deflater->Deflate();
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return scope.Close(Boolean::New(deflater->IsOutputBufferFull()));
   }
   
   static Handle<Value> Finish(const Arguments& args) {
      Deflater *deflater = ObjectWrap::Unwrap<Deflater>(args.This());
      HandleScope scope;
      
      if (deflater->HasFinished()) {
         return scope.Close(False());
      }
      
      int r = deflater->Finish();
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return scope.Close(True());
   }
   
   static Handle<Value> GetOutput(const Arguments& args) {
      Deflater *deflater = ObjectWrap::Unwrap<Deflater>(args.This());
      HandleScope scope;
      
      Buffer* slowBuffer = Buffer::New(deflater->GetOutput(), deflater->GetOutputLength());
      deflater->ResetOutput();
      
      if (deflater->HasFinished()) {
         free(deflater->GetOutput());
      }
      
      Local<Object> globalObj = Context::GetCurrent()->Global();
      Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
      Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(deflater->GetOutputLength()), Integer::New(0) };
      Local<Object> jsBuffer = bufferConstructor->NewInstance(3, constructorArgs);
      
      return scope.Close(jsBuffer);
   }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////

class Inflater : ObjectWrap {

  private:
   z_stream strm;
   char* input;
   char* output;
   int output_length;
   int status;
   bool finished;
   int output_chunk_size;
   
  public:

   Inflater() {
      input = NULL;
      output_length = 0;
      output = NULL;
      finished = false;
   }

   int Init(int windowBits, int chunk_size) {
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      output_chunk_size = chunk_size;
      return inflateInit2(&strm, windowBits);
   }
   
   int SetInput(int in_length) {
      strm.avail_in = in_length;
      strm.next_in = (Bytef*) input;
      
      if (output == NULL || output_length == output_chunk_size) {
         strm.avail_out = output_chunk_size;
         output = (char*) malloc(output_chunk_size);
         
         if (output == NULL) {
            return Z_MEM_ERROR;
         }
         
         strm.next_out = (Bytef*) output;
      }
      
      return Z_OK;
   }
   
   void ResetOutput() {
      strm.avail_out = output_chunk_size;
      strm.next_out = (Bytef*) output;
   }
   
   int Inflate() {
      int ret = inflate(&strm, Z_NO_FLUSH);
      status = ret;
      output_length = output_chunk_size - strm.avail_out;

      if (ret == Z_STREAM_END) {
         inflateEnd(&strm);
         finished = true;
      }

      return ret;
   }
   
   bool IsOutputBufferFull() {
      return output_length == output_chunk_size;
   }
   
   bool HasFinished() {
      return finished;
   }
   
   int GetOutputLength() {
      return output_length;
   }
   
   char* GetOutput() {
      return output;
   }
   
   // node.js wrapper //
       
   static void Init(v8::Handle<v8::Object> target) {
      Local<FunctionTemplate> t = FunctionTemplate::New(New);

      t->InstanceTemplate()->SetInternalFieldCount(1);
      t->SetClassName(String::NewSymbol("Inflater"));

      NODE_SET_PROTOTYPE_METHOD(t, "write", SetInput);
      NODE_SET_PROTOTYPE_METHOD(t, "inflate", Inflate);
      NODE_SET_PROTOTYPE_METHOD(t, "read", GetOutput);
      
      target->Set(String::NewSymbol("Inflater"), t->GetFunction());
   }
    
   static Handle<Value> New (const Arguments& args) {
      HandleScope scope;
      
      int windowBits = 16 + MAX_WBITS; // gzip
      int output_chunk_size = 65545; // <--- TODO: should have optional output buffer size
      
      if (args.Length() > 0) {
         if (args[0]->IsString()) {
            char* strLevel = *String::AsciiValue(args[0]->ToString());

            if (strcmp(strLevel, "gzip") == 0) {
               windowBits = 16 + MAX_WBITS;
            }
            else if (strcmp(strLevel, "zlib") == 0) {
               windowBits = MAX_WBITS;
            }
            else if (strcmp(strLevel, "deflate") == 0) {
               windowBits = -MAX_WBITS;
            }
            else {
               Local<Value> exception = Exception::TypeError(String::New("bad deflate kind"));
               return ThrowException(exception);
            }
         }
         else if (args[0]->IsUndefined()) {
         }
         else {
            Local<Value> exception = Exception::TypeError(String::New("expected a Number or String"));
            return ThrowException(exception);
         }
      }
      
      Inflater *inflater = new Inflater();
      inflater->Wrap(args.This());

      int r = inflater->Init(windowBits, output_chunk_size);
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return args.This();
   }
   
   static Handle<Value> SetInput(const Arguments& args) {
      Inflater *inflater = ObjectWrap::Unwrap<Inflater>(args.This());
      HandleScope scope;
      
      Local<Object> in = args[0]->ToObject();
      ssize_t length = Buffer::Length(in);

      // copy the input buffer, because it can be kept for several deflate() calls
      inflater->input = (char*)realloc(inflater->input, length);
      
      if (inflater->input == NULL) {
          return GetZError(Z_MEM_ERROR);
      }
      
      memcpy(inflater->input, Buffer::Data(in), length);

      int r = inflater->SetInput(length);
      
      if (r < 0) {
         return GetZError(r);
      }
      
      return scope.Close(Undefined());
   }
   
   static Handle<Value> Inflate(const Arguments& args) {
      Inflater *inflater = ObjectWrap::Unwrap<Inflater>(args.This());
      HandleScope scope;

      if (inflater->HasFinished()) {
         return scope.Close(False());
      }

      int r = inflater->Inflate();
      
      if (r < 0) {
         return GetZError(r);
      }
      
      if (inflater->HasFinished()) {
         return scope.Close(True());
      }
      else {
         return scope.Close(Boolean::New(inflater->IsOutputBufferFull()));
      }
   }
   
   static Handle<Value> GetOutput(const Arguments& args) {
      Inflater *inflater = ObjectWrap::Unwrap<Inflater>(args.This());
      HandleScope scope;
      
      Buffer* slowBuffer = Buffer::New(inflater->GetOutput(), inflater->GetOutputLength());
      inflater->ResetOutput();
      
      if (inflater->HasFinished()) {
         free(inflater->GetOutput());
      }
      
      Local<Object> globalObj = Context::GetCurrent()->Global();
      Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
      Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(inflater->GetOutputLength()), Integer::New(0) };
      Local<Object> jsBuffer = bufferConstructor->NewInstance(3, constructorArgs);
      
      return scope.Close(jsBuffer);
   }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////

static Handle<Value> GetVersion(const Arguments &args) {
   const char* version = zlibVersion();
   return String::New(version);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

static Handle<Value> OnePassDeflate(const Arguments& args) {
   HandleScope scope;
   
   int level = Z_DEFAULT_COMPRESSION;
   int windowBits = 16 + MAX_WBITS; // gzip
   int memLevel = 8;
   int strategy = Z_DEFAULT_STRATEGY;
   
   int idx = 1;
   
   if (args.Length() > 0) {
      if(args[idx+0]->IsNumber()) {
         level = args[idx+0]->Int32Value();        
         if (level < 0 || level > 9) {
            Local<Value> exception = Exception::Error(String::New("level must be between 0 and 9"));
            return ThrowException(exception);
         }
      }
      else if (args[idx+0]->IsString()) {
         char* strLevel = *String::AsciiValue(args[idx+0]->ToString());
         
         if (strcmp(strLevel, "gzip") == 0) {
            windowBits = 16 + MAX_WBITS;
         }
         else if (strcmp(strLevel, "zlib") == 0) {
            windowBits = MAX_WBITS;
         }
         else if (strcmp(strLevel, "deflate") == 0) {
            windowBits = -MAX_WBITS;
         }
         else {
            Local<Value> exception = Exception::TypeError(String::New("bad deflate kind"));
            return ThrowException(exception);
         }
      }
      else if (args[idx+0]->IsUndefined()) {
      }
      else {
         Local<Value> exception = Exception::TypeError(String::New("expected a Number or String"));
         return ThrowException(exception);
      }
      
      if (args.Length() > 1) {
         if(args[idx+1]->IsNumber()) {
            level = args[idx+1]->Int32Value();        
            if (level < 0 || level > 9) {
               Local<Value> exception = Exception::Error(String::New("level must be between 0 and 9"));
               return ThrowException(exception);
            }
         }
         else if (args[idx+1]->IsUndefined()) {
         }
         else {
            Local<Value> exception = Exception::TypeError(String::New("expected a Number"));
            return ThrowException(exception);
         }
      }
   }
   
   Local<Object> inBuff = args[0]->ToObject();
   char* in = Buffer::Data(inBuff);
   size_t in_length = Buffer::Length(inBuff);

   z_stream strm;
   strm.zalloc = Z_NULL;
   strm.zfree = Z_NULL;
   strm.opaque = Z_NULL;
   int r =  deflateInit2(&strm, level, Z_DEFLATED, windowBits, memLevel, strategy);
      
   if (r < 0) {
      return GetZError(r);
   }
   
   // deflate
   strm.avail_in = in_length;
   strm.next_in = (Bytef*) in;
   
   uLong bound = deflateBound(&strm, strm.avail_in);
   
   char* out = (char*) malloc(bound);
   
   if (out == NULL) {
      return GetZError(Z_MEM_ERROR);
   }
   
   strm.avail_out = bound;
   strm.next_out = (Bytef*) out;
          
   r = deflate(&strm, Z_FINISH);
   
   int out_length = bound - strm.avail_out;
   
   deflateEnd(&strm);

   if (r < 0) {
      return GetZError(r);
   }
   
   // output
   Buffer* slowBuffer = Buffer::New(out, out_length);
   free(out);

   Local<Object> globalObj = Context::GetCurrent()->Global();
   Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
   Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(out_length), Integer::New(0) };
   Local<Object> jsBuffer = bufferConstructor->NewInstance(3, constructorArgs);

   return scope.Close(jsBuffer);
}
   
static Handle<Value> OnePassInflate(const Arguments& args) {
   HandleScope scope;
   
   int windowBits = 16 + MAX_WBITS; // gzip
   
   int idx = 1;
   
   if (args.Length() > 0) {
      if (args[idx+0]->IsString()) {
         char* strLevel = *String::AsciiValue(args[idx+0]->ToString());
         
         if (strcmp(strLevel, "gzip") == 0) {
            windowBits = 16 + MAX_WBITS;
         }
         else if (strcmp(strLevel, "zlib") == 0) {
            windowBits = MAX_WBITS;
         }
         else if (strcmp(strLevel, "deflate") == 0) {
            windowBits = -MAX_WBITS;
         }
         else {
            Local<Value> exception = Exception::TypeError(String::New("bad deflate kind"));
            return ThrowException(exception);
         }
      }
      else if (args[idx+0]->IsUndefined()) {
      }
      else {
         Local<Value> exception = Exception::TypeError(String::New("expected a String"));
         return ThrowException(exception);
      }
   }
   
   Local<Object> inBuff = args[0]->ToObject();
   char* in = Buffer::Data(inBuff);
   size_t in_length = Buffer::Length(inBuff);

   if (in_length == 0) {
      Local<Value> exception = Exception::TypeError(String::New("Buffer length must be greater than zero"));
      return ThrowException(exception);
   }

   z_stream strm;
   strm.zalloc = Z_NULL;
   strm.zfree = Z_NULL;
   strm.opaque = Z_NULL;
   strm.avail_in = 0;
   strm.next_in = Z_NULL;
   int r = inflateInit2(&strm, windowBits);
      
   if (r < 0) {
      return GetZError(r);
   }
   
   // deflate
   strm.avail_in = in_length;
   strm.next_in = (Bytef*) in;
   
   // urgh, we don't know the buffer size (Q: is it in the gzip header?)
   uLong bound = 131072; // 128K
   
   char* out = (char*) malloc(bound);
   
   if (out == NULL) {
      return GetZError(Z_MEM_ERROR);
   }
   
   strm.avail_out = bound;
   strm.next_out = (Bytef*) out;
          
   r = inflate(&strm, Z_FINISH);
   
   while (r == Z_BUF_ERROR) {
      bound = bound * 2;
      size_t len = (char*)strm.next_out - out;
      
      out = (char*) realloc(out, bound);

      if (out == NULL) {
         return GetZError(Z_MEM_ERROR);
      }
      
      strm.avail_out = bound - len;
      strm.next_out = (Bytef*) (out + len);
      
      r = inflate(&strm, Z_FINISH);
   }
   
   if (r < 0) {
      return GetZError(r);
   }
   
   int out_length = bound - strm.avail_out;
   
   inflateEnd(&strm);
   
   // output
   Buffer* slowBuffer = Buffer::New(out, out_length);
   free(out);

   Local<Object> globalObj = Context::GetCurrent()->Global();
   Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
   Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(out_length), Integer::New(0) };
   Local<Object> jsBuffer = bufferConstructor->NewInstance(3, constructorArgs);

   return scope.Close(jsBuffer);
}
   
////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
   void init (Handle<Object> target) 
   {
     Deflater::Init(target);
     Inflater::Init(target);
     
     NODE_SET_METHOD(target, "version", GetVersion);
     NODE_SET_METHOD(target, "deflate", OnePassDeflate);
     NODE_SET_METHOD(target, "inflate", OnePassInflate);
   }
   
   NODE_MODULE(compress, init);
}