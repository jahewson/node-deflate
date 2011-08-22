# deflate -- gzip, zlib, and deflate for node.js
Streaming [DEFLATE](http://tools.ietf.org/html/rfc1951) (RFC 1951), [ZLIB](http://tools.ietf.org/html/rfc1950)
(RFC 1950) and [GZIP](http://tools.ietf.org/html/rfc1952) (RFC 1952) compression and expansion for
[node.js](http://nodejs.org/).

Implemented as a C++ wrapper around **[zlib](http://zlib.net)** ![zlib](http://swik.net/swikIcons/img-144-96x96.png)

## Install

    npm install deflate

or from source:

    git clone git://github.com/jahewson/node-deflate.git
    cd node-deflate
    ./build.sh

## Examples

You'll need to start with:

    var deflate = require('deflate');

### String

Deflate a `String`, returning a `Buffer`:

	var gzip = deflate.deflate('some text which needs to be compressed');

### Buffer

Deflate a `Buffer` which contains all data to be compressed:

	var gzip = deflate.deflate(fs.ReadFileSync('example.txt'));
	
Inflate a `Buffer` which contains all compressed data:

	var data = deflate.inflate(fs.ReadFileSync('example.gz'));

### Stream

Any `ReadableStream` can be wrapped with a `DeflateStream`, which we can `pipe` into a `WritableStream`.

	// wrap input stream with deflate
	var input = fs.createReadStream('example.gz');
	input = deflate.createDeflateStream(input);
	
	// pipe to output stream
	var output = fs.createWriteStream('example.txt');
	input.pipe(output);
	
Any `ReadableStream` can be wrapped with an `InflateStream`, which we can `pipe` into a `WritableStream`.

	...

### Low-level API

If the `Stream` interface doesn't meet your needs, you can access the low-level API directly, via
`Deflater` and `Inflater` classes. See the implementation of `DelateStream` and `InflateStream` to learn how to use
the low-level API. All low-level functions are synchronous.
	
## Options

### deflate(level=6)
Use `gzip` format. `level` sets the compression level from `0` (uncompressed) to `9` (highly compressed), 
a smaller number is faster.

### deflate(format='gzip', level=6)
`format` specifies the format and can be `gzip`, `zlib`, or `deflate`.

`level` sets the compression level from `0` (uncompressed) to `9` (highly compressed), 
a smaller number is faster.

---

### createDeflateStream(level=6)
Use `gzip` format. `level` sets the compression level from `0` (uncompressed) to `9`, a smaller number is faster.

### createDeflateStream(format='gzip', level=6, bufferSize=131072)
`format` specifies the format and can be `gzip`, `zlib`, or `deflate`.

`level` sets the compression level from `0` (uncompressed) to `9` (highly compressed), 
a smaller number is faster.

`bufferSize` is the size of the output buffer in bytes. Output data is buffered until it reaches
this size, with the exception of the final chunk. The default is 128K.

## Tests
Tests require [nodeunit](https://github.com/caolan/nodeunit), you can run them all with:
	
	cd node-deflate
    nodeunit test
	
## Background

There are many gzip modules for node.js, some just wrap the command-line `gzip`/`gunzip`,
others wrap zlib, but none are mature or well-maintained, so I decided to
invest some time to write node-deflate. Hopefully you'll find it useful.

## Acknowledgements

The starting point for node-deflate was the zlib [Usage Example](http://zlib.net/zpipe.c)
public domain C code by Mark Adler (see [Adler32](http://en.wikipedia.org/wiki/Adler-32)).