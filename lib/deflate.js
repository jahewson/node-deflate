var deflate = require('../build/Release/deflate-bindings'),
    Stream = require('stream').Stream,
     util = require('util');

module.exports = deflate;

//////////////////////////////////////////////////////////

module.exports.createDeflateStream = function(readStream, format, level, bufferSize) {
  if (!readStream) {
    throw new Error('expected ReadableStream');
  }
  return new DeflateStream(readStream, format, level, bufferSize);
};

function DeflateStream(readStream, format, level, bufferSize) {
  this.writable = true;
  this.readable = true;
  this.deflater = new deflate.Flater(false, format, level, bufferSize);
  this.readStream = readStream;
  
  if (readStream) {
    if (!readStream.readable) {
      throw new Error('expected ReadableStream');
    }
  }
}
util.inherits(DeflateStream, Stream);

DeflateStream.prototype.write = function(data) {
  if (!Buffer.isBuffer(data)) {
    throw new Error('expected Buffer');
  }
  this.deflater.write(data);
  while (this.deflater.deflate()) {
    this.emit('data', this.deflater.read());
  }
};

DeflateStream.prototype.end = function() {
  while (this.deflater.deflateFlush()) {
    this.emit('data', this.deflater.read());
  }
  this.emit('end');
};

DeflateStream.prototype.destroy = function() {
  this.emit('close');
};

DeflateStream.prototype.pipe = function(src) {
  Stream.prototype.pipe.call(this, src);
  if (this.readStream) {
    this.readStream.pipe(this);
  }
};

//////////////////////////////////////////////////////////

module.exports.createInflateStream = function(readStream, format, level, bufferSize) {
  if (!readStream) {
    throw new Error('expected ReadableStream');
  }
  return new InflateStream(readStream, format, level, bufferSize);
};

function InflateStream(readStream, format, level, bufferSize) {
  this.writable = true;
  this.readable = true;
  this.inflater = new deflate.Flater(true, format, level, bufferSize);
  this.readStream = readStream;
  
  if (readStream) {
    if (!readStream.readable) {
      throw new Error('expected ReadableStream');
    }
  }
}
util.inherits(InflateStream, Stream);

InflateStream.prototype.write = function(data) {
  if (!Buffer.isBuffer(data)) {
    throw new Error('expected Buffer');
  }
  this.inflater.write(data);
  while (this.inflater.inflate()) {
    this.emit('data', this.inflater.read());
  }
};

InflateStream.prototype.end = function() {
  this.emit('end');
};

InflateStream.prototype.destroy = function() {
  this.emit('close');
};

InflateStream.prototype.pipe = function(src) {
  Stream.prototype.pipe.call(this, src);
  if (this.readStream) {
    this.readStream.pipe(this);
  }
};

//////////////////////////////////////////////////////////