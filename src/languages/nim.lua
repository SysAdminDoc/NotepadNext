local L = {}

L.lexer = "nim"

L.singleLineComment = "# "

L.extensions = {
	"nim",
}

L.keywords = {
	[0] = "addr and as asm atomic bind block break case cast concept const continue converter defer defined destructor distinct do elif else end enum except export finally for from func generic if import in include interface is isnot iterator let macro method mixin mod nil not notin object of or out proc ptr raise ref return shl sink static template threadvar try tuple type using var when where while with without xor yield",
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
	["COMMENT DOC"] = {
		id = 2,
		fgColor = rgb(0x3F703F),
		bgColor = rgb(0xFFFFFF),
	},
	["COMMENT LINE"] = {
		id = 3,
		fgColor = rgb(0x008000),
		bgColor = rgb(0xFFFFFF),
	},
	["COMMENT LINE DOC"] = {
		id = 4,
		fgColor = rgb(0x3F703F),
		bgColor = rgb(0xFFFFFF),
	},
	["NUMBER"] = {
		id = 5,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING"] = {
		id = 6,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["CHARACTER"] = {
		id = 7,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["KEYWORD"] = {
		id = 8,
		fgColor = rgb(0x00007F),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["TRIPLE STRING"] = {
		id = 9,
		fgColor = rgb(0x7F0000),
		bgColor = rgb(0xFFFFFF),
	},
	["TRIPLE DOUBLE STRING"] = {
		id = 10,
		fgColor = rgb(0x7F0000),
		bgColor = rgb(0xFFFFFF),
	},
	["BACKTICK IDENTIFIER"] = {
		id = 11,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["FUNCTION NAME"] = {
		id = 12,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["STRING EOL"] = {
		id = 13,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xE0C0E0),
	},
	["NUMBER ERROR"] = {
		id = 14,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xE0C0E0),
	},
	["OPERATOR"] = {
		id = 15,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["IDENTIFIER"] = {
		id = 16,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
}

return L
