var deflate = require('./deflate-bindings'),
    Stream = require('stream').Stream,
     util = require('util');

module.exports = deflate;

//////////////////////////////////////////////////////////
// TODO: REFACTOR THIS TO MATCH MY LineReader STREAM ????
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
  this.deflater = new deflate.Deflater(format, level, bufferSize);
  this.readStream = readStream;
  
  if (readStream) {
    if (!readStream.readable) {
      throw new Error('expected ReadableStream');
    }
  }
}
util.inherits(DeflateStream, Stream);

DeflateStream.prototype.write = function(data) {
  this.deflater.write(data);
  while (this.deflater.deflate()) {
    this.emit('data', this.deflater.read());
  }
};

DeflateStream.prototype.end = function() {
  while (this.deflater.flush()) {
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
  this.inflater = new deflate.Inflater(format, level, bufferSize);
  this.readStream = readStream;
  
  if (readStream) {
    if (!readStream.readable) {
      throw new Error('expected ReadableStream');
    }
  }
}
util.inherits(InflateStream, Stream);

InflateStream.prototype.write = function(data) {
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
