enum BinType {
	OBJECT_8 = 1,
	OBJECT_64,
	ARRAY_8,
	ARRAY_64,
	STRING_8, // A string that uses 8-bits as length
	STRING_64, // A string that uses 64-bits as length
	NULL_T,
	BOOLEAN_TRUE,
	BOOLEAN_FALSE,
	NUMBER_FLOAT,
	NUMBER_INTEGER_8,
	NUMBER_INTEGER_16,
	NUMBER_INTEGER_32,
	NUMBER_INTEGER_64,
	NUMBER_UNSIGNED_8,
	NUMBER_UNSIGNED_16,
	NUMBER_UNSIGNED_32,
	NUMBER_UNSIGNED_64,
};

const U8Min = 0;
const U8Max = 255;
const U16Min = 0;
const U16Max = 65535;
const U32Min = 0;
const U32Max = 4294967295;
const I8Min = -128;
const I8Max = 127;
const I16Min = -32768;
const I16Max = 32767;
const I32Min = -2147483648;
const I32Max = 2147483647;

const isSafeInteger = (n: number) => {
    return Number.isSafeInteger(n);
}

const inRangeU8 = (n: number) => {
    return (U8Min <= n) && (n <= U8Max);
}
const inRangeU16 = (n: number) => {
    return (U16Min <= n) && (n <= U16Max);
}
const inRangeU32 = (n: number) => {
    return (U32Min <= n) && (n <= U32Max);
}
const inRangeI8 = (n: number) => {
    return (I8Min <= n) && (n <= I8Max);
}
const inRangeI16 = (n: number) => {
    return (I16Min <= n) && (n <= I16Max);
}
const inRangeI32 = (n: number) => {
    return (I32Min <= n) && (n <= I32Max);
}

class ByteArray {
    private capacity: number = 1;
    private length: number = 0;
    private bytes: Uint8Array = new Uint8Array(this.capacity);
    private checkForDouble(addition: number) {
        if (this.length + addition >= this.capacity) {
            this.capacity = 2 * this.capacity + addition;
            const newArray = new Uint8Array(this.capacity); 
            newArray.set(this.bytes);
            this.bytes = newArray;
        }
    }
    private readUint8Array(array: Uint8Array) {
        this.checkForDouble(array.length);
        for (let i = 0; i < array.length; i++) {
            this.bytes[this.length] = array[i];
            this.length += 1;
        }
    }
    private floatArray = new Float64Array(1);
    private bigArray = new BigUint64Array(1);
    private u16Array = new Uint16Array(1);
    private u32Array = new Uint32Array(1);
    constructor() {}
    public push_back_u8(object: number) {
        this.checkForDouble(1);
        this.bytes[this.length] = object & 0xFF;
        this.length += 1;
    }
    public push_back_u16(object: number) {
        this.u16Array[0] = object;
        const sarr = new Uint8Array(this.u16Array.buffer);
        this.readUint8Array(sarr);
    }
    public push_back_u32(object: number) {
        this.u32Array[0] = object;
        const sarr = new Uint8Array(this.u32Array.buffer);
        this.readUint8Array(sarr);
    }
    public push_back_u64(object: number) {
        const big = BigInt(object);
        this.bigArray[0] = big;
        const sarr = new Uint8Array(this.bigArray.buffer);
        this.readUint8Array(sarr);
    }
    public push_back_i8(object: number) {
        this.push_back_u8(object);
    }
    public push_back_i16(object: number) {
        this.push_back_u16(object);
    }
    public push_back_i32(object: number) {
        this.push_back_u32(object);
    }
    public push_back_i64(object: number) {
        this.push_back_u64(object);
    }
    public push_back_float(object: number) {
        this.floatArray[0] = object;
        const sarr = new Uint8Array(this.floatArray.buffer);
        this.readUint8Array(sarr);
    }
    public get(): ArrayBuffer {
        const a = new Uint8Array(this.length);
        for (let i = 0; i < this.length; i++) {
            a[i] = this.bytes[i];
        }
        return a.buffer;
    }
    public getLength(): number {
        return this.length;
    }
};



const serialize_Impl = (json: any, bytes: ByteArray, depth: number) => {
    if (depth > 32) {
        throw new Error("Recursive depth too great. Either the json has a circular reference, or is highly nested.");
    }
    const type = typeof json;
    switch (type) {
        case "object":
            if (json === null) {
                bytes.push_back_u8(BinType.NULL_T);
            } else if (Array.isArray(json)) {
                const typed = json as any[];
                const length = typed.length;
                if (inRangeU8(length)) {
                    bytes.push_back_u8(BinType.ARRAY_8);
                    bytes.push_back_u8(length);
                } else {
                    bytes.push_back_u8(BinType.ARRAY_64);
                    bytes.push_back_u64(length);
                }
                for (const element of typed) {
                    serialize_Impl(element, bytes, depth + 1);
                }
            } else {
                const typed = json as Object;
                const entries = Object.entries(typed);
                const length = entries.length;
                if (inRangeU8(length)) {
                    bytes.push_back_u8(BinType.OBJECT_8);
                    bytes.push_back_u8(length);
                } else {
                    bytes.push_back_u8(BinType.OBJECT_64);
                    bytes.push_back_u64(length);
                }
                for (const [ key, value ] of entries) {
                    serialize_Impl(key, bytes, depth + 1);
                    serialize_Impl(value, bytes, depth + 1);
                }
            }
            break;
        case "string":
            {
                const typed = json as string;
                const length = typed.length;
                if (inRangeU8(length)) {
                    bytes.push_back_u8(BinType.STRING_8);
                    bytes.push_back_u8(length);
                } else {
                    bytes.push_back_u8(BinType.STRING_64);
                    bytes.push_back_u64(length);
                }
                if (length !== 0) {
                    const array = new TextEncoder().encode(typed);
                    for (const character of array) {
                        bytes.push_back_u8(character);
                    }
                }
            }
            break;
        case "boolean":
            {
                const typed = json as boolean;
                if (typed) {
                    bytes.push_back_u8(BinType.BOOLEAN_TRUE);
                } else {
                    bytes.push_back_u8(BinType.BOOLEAN_FALSE);
                }
            }
            break;
        case "number":
            {
                const typed = json as number;
                if (isSafeInteger(typed)) {
                    if (typed >= 0) {
                        // Unsigned
                        if (inRangeU8(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_UNSIGNED_8);
                            bytes.push_back_u8(typed);
                        } else if (inRangeU16(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_UNSIGNED_16);
                            bytes.push_back_u16(typed);
                        } else if (inRangeU32(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_UNSIGNED_32);
                            bytes.push_back_u32(typed);
                        } else {
                            bytes.push_back_u8(BinType.NUMBER_UNSIGNED_64);
                            bytes.push_back_u64(typed);
                        }
                    } else { 
                        // Signed
                        if (inRangeI8(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_INTEGER_8);
                            bytes.push_back_i8(typed);
                        } else if (inRangeI16(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_INTEGER_16);
                            bytes.push_back_i16(typed);
                        } else if (inRangeI32(typed)) {
                            bytes.push_back_u8(BinType.NUMBER_INTEGER_32);
                            bytes.push_back_i32(typed);
                        } else {
                            bytes.push_back_u8(BinType.NUMBER_INTEGER_64);
                            bytes.push_back_i64(typed);
                        }
                    }
                } else {
                    bytes.push_back_u8(BinType.NUMBER_FLOAT);
                    bytes.push_back_float(typed);
                }
            }
            break;
        default:
            throw new Error("Type is not valid in json.");
    }
}
const serialize = (json: any): ArrayBuffer => {
    const byteArray = new ByteArray();
    serialize_Impl(json, byteArray, 0);
    return byteArray.get();
}

const checkForLength = (bytes: Uint8Array, index: number, length: number): void => {
    if (bytes.length - index < length) {
        throw new Error(`Not enough requested bytes.`);
    }
}
const readBytesAsU8 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 1);
    const view = new DataView(bytes.buffer, index.value, 1);
    const value = view.getUint8(0);
    index.value += 1;
    return value;
}
const readBytesAsU16 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 2);
    const view = new DataView(bytes.buffer, index.value, 2);
    const value = view.getUint16(0);
    index.value += 2;
    return value;
}
const readBytesAsU32 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 4);
    const view = new DataView(bytes.buffer, index.value, 4);
    const value = view.getUint32(0);
    index.value += 4;
    return value;
}
const readBytesAsU64 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 8);
    const view = new DataView(bytes.buffer, index.value, 8);
    const value = view.getBigUint64(0);
    index.value += 8;
    return Number(value);
}
const readBytesAsI8 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 1);
    const view = new DataView(bytes.buffer, index.value, 1);
    const value = view.getInt8(0);
    index.value += 1;
    return value;
}
const readBytesAsI16 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 2);
    const view = new DataView(bytes.buffer, index.value, 2);
    const value = view.getInt16(0);
    index.value += 2;
    return value;
}
const readBytesAsI32 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 4);
    const view = new DataView(bytes.buffer, index.value, 4);
    const value = view.getInt32(0);
    index.value += 4;
    return value;
}
const readBytesAsI64 = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 8);
    const view = new DataView(bytes.buffer, index.value, 8);
    const value = view.getBigInt64(0);
    index.value += 8;
    return Number(value);
}
const readBytesAsString = (bytes: Uint8Array, index: { value: number }, length: number): string => {
    checkForLength(bytes, index.value, length);
    const view = new DataView(bytes.buffer, index.value, length);
    index.value += length;
    return new TextDecoder().decode(view);
}
const readBytesAsFloat = (bytes: Uint8Array, index: { value: number }): number => {
    checkForLength(bytes, index.value, 8);
    const view = new DataView(bytes.buffer, index.value, 8);
    const value = view.getFloat64(0, true);
    index.value += 8;
    return Number(value);
}
const deserialize_Impl = (bytes: Uint8Array, index: { value: number }, depth: number): Object|string|number|null|boolean => {
    if (depth > 32) {
        throw new Error(`Recursive depth too deep.`);
    }
    const type = readBytesAsU8(bytes, index) as BinType;
    switch (type) {
        case BinType.OBJECT_8:
            {
                const object: any = {};
                const length = readBytesAsU8(bytes, index);
                for (let i = 0; i < length; i++) {
                    // Read string
                    const key = deserialize_Impl(bytes, index, depth + 1);
                    if (typeof key !== "string") {
                        throw new Error(`Objects must have strings are keys.`);
                    }
                    // Read the value
                    const value = deserialize_Impl(bytes, index, depth + 1);
                    object[key] = value;
                }
                return object;
            }
        case BinType.OBJECT_64:
            {
                const object: any = {};
                const length = readBytesAsU64(bytes, index);
                for (let i = 0; i < length; i++) {
                    // Read string
                    const key = deserialize_Impl(bytes, index, depth + 1);
                    if (typeof key !== "string") {
                        throw new Error(`Objects must have strings are keys.`);
                    }
                    // Read the value
                    const value = deserialize_Impl(bytes, index, depth + 1);
                    object[key] = value;
                }
                return object;
            }
        case BinType.ARRAY_8:
            {
                const array: any[] = [];
                const length = readBytesAsU8(bytes, index);
                for (let i = 0; i < length; i++) {
                    // Read each element
                    const element = deserialize_Impl(bytes, index, depth + 1);
                    array.push(element);
                }
                return array;
            }
        case BinType.ARRAY_64:
            {
                const array: any[] = [];
                const length = readBytesAsU64(bytes, index);
                for (let i = 0; i < length; i++) {
                    // Read each element
                    const element = deserialize_Impl(bytes, index, depth + 1);
                    array.push(element);
                }
                return array;
            }
        case BinType.STRING_8:
            {
                const length = readBytesAsU8(bytes, index);
                return readBytesAsString(bytes, index, length);
            }
        case BinType.STRING_64:
            {
                const length = readBytesAsU64(bytes, index);
                return readBytesAsString(bytes, index, length);
            }
        case BinType.NULL_T:
            return null;
        case BinType.BOOLEAN_TRUE:
            return true;
        case BinType.BOOLEAN_FALSE:
            return false;
        case BinType.NUMBER_FLOAT:
            return readBytesAsFloat(bytes, index);
        case BinType.NUMBER_INTEGER_8:
            return readBytesAsI8(bytes, index);
        case BinType.NUMBER_INTEGER_16:
            return readBytesAsI16(bytes, index);
        case BinType.NUMBER_INTEGER_32:
            return readBytesAsI32(bytes, index);
        case BinType.NUMBER_INTEGER_64:
            return readBytesAsI64(bytes, index);
        case BinType.NUMBER_UNSIGNED_8:
            return readBytesAsU8(bytes, index);
        case BinType.NUMBER_UNSIGNED_16:
            return readBytesAsU16(bytes, index);
        case BinType.NUMBER_UNSIGNED_32:
            return readBytesAsU32(bytes, index);
        case BinType.NUMBER_UNSIGNED_64:
            return readBytesAsU64(bytes, index);
        default:
            throw new Error(`Malformed json. Received: ${type}.`);
    }
}
const deserialize = (array: ArrayBuffer): Object | string | number | null | boolean => {
    const bytes = new Uint8Array(array);
    const index = {
        value: 0,
    };
    return deserialize_Impl(bytes, index, 0);
}

export default { serialize, deserialize };