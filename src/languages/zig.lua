local L = {}

L.lexer = "zig"

L.singleLineComment = "// "

L.extensions = {
	"zig",
}

L.keywords = {
	[0] = "addr align allowzero and anyframe anytype asm async await break call catch comptime const continue defer else enum errdefer error export extern fn for if inline noalias noinline nosuspend opaque or orelse packed pub resume return linksection struct suspend switch test threadlocal try union unreachable using var volatile while",
	[1] = "atomic cmpxchg enum literal noalias noreturn packed suspend threadlocal",
	[2] = "anyerror anyopaque bool c_int c_long c_longdouble c_longlong c_short c_uint c_ulong c_ulonglong c_ushort f16 f32 f64 f80 f128 i8 i16 i32 i64 i128 isize noreturn u8 u16 u32 u64 u128 usize void",
	[3] = "ArrayList AutoHashMap EnumArray HashMap StringHashMap",
}

L.styles = {
	["DEFAULT"] = {
		id = 0,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["COMMENT"] = {
		id = 1,
		fgColor = rgb(0x008000),
		bgColor = rgb(0xFFFFFF),
	},
	["DOC COMMENT"] = {
		id = 2,
		fgColor = rgb(0x3F703F),
		bgColor = rgb(0xFFFFFF),
	},
	["TOP DOC COMMENT"] = {
		id = 3,
		fgColor = rgb(0x3F703F),
		bgColor = rgb(0xFFFFFF),
	},
	["NUMBER"] = {
		id = 4,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
	},
	["OPERATOR"] = {
		id = 5,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["CHARACTER"] = {
		id = 6,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING"] = {
		id = 7,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["MULTILINE STRING"] = {
		id = 8,
		fgColor = rgb(0x7F0000),
		bgColor = rgb(0xFFFFFF),
	},
	["ESCAPE SEQUENCE"] = {
		id = 9,
		fgColor = rgb(0x0000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["IDENTIFIER"] = {
		id = 10,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["FUNCTION"] = {
		id = 11,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["BUILTIN FUNCTION"] = {
		id = 12,
		fgColor = rgb(0x800080),
		bgColor = rgb(0xFFFFFF),
	},
	["PRIMARY KEYWORD"] = {
		id = 13,
		fgColor = rgb(0x00007F),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["SECONDARY KEYWORD"] = {
		id = 14,
		fgColor = rgb(0x00007F),
		bgColor = rgb(0xFFFFFF),
	},
	["TERTIARY KEYWORD"] = {
		id = 15,
		fgColor = rgb(0x00007F),
		bgColor = rgb(0xFFFFFF),
	},
	["GLOBAL TYPE"] = {
		id = 16,
		fgColor = rgb(0x8000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["IDENTIFIER STRING"] = {
		id = 17,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING EOL"] = {
		id = 18,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xE0C0E0),
	},
}

return L
