# JsonBinary
Convert to and from json as a binary file. 

A C++ and a TypeScript version of the same serializer/deserializer can be found in this repository.

Performance has not yet been measured against each languages' JSON parser/stringifier.

But the serialized binary jsons are significantly smaller than text json files.

The two implementations are compatible, although the TypeScript version will have problems with absolute large 64-bit numbers.
This is a language limitation of JS.

