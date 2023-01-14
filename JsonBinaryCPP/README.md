# JsonBinaryCPP

This is a header that allows to serialize and deserialize JSON to and from a binary format.

Serializers for important classes such as vector and map are provided. If you wish to add your own you must provide a specialization of your type for Serialization::serialize_impl.

Deserialization can only be done into a JSON object.
