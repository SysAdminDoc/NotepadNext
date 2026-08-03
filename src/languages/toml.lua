local L = {}

L.lexer = "toml"

L.singleLineComment = "# "

L.extensions = {
	"toml",
}

L.keywords = {
	[0] = "false inf nan true",
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
	["IDENTIFIER"] = {
		id = 2,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["KEYWORD"] = {
		id = 3,
		fgColor = rgb(0x00007F),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["NUMBER"] = {
		id = 4,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
	},
	["TABLE"] = {
		id = 5,
		fgColor = rgb(0x0000FF),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["KEY"] = {
		id = 6,
		fgColor = rgb(0x8000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["ERROR"] = {
		id = 7,
		fgColor = rgb(0xFF0000),
		bgColor = rgb(0xFFE0E0),
	},
	["OPERATOR"] = {
		id = 8,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["SINGLE QUOTED STRING"] = {
		id = 9,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["DOUBLE QUOTED STRING"] = {
		id = 10,
		fgColor = rgb(0x7F007F),
		bgColor = rgb(0xFFFFFF),
	},
	["TRIPLE SINGLE QUOTED STRING"] = {
		id = 11,
		fgColor = rgb(0x7F0000),
		bgColor = rgb(0xFFFFFF),
	},
	["TRIPLE DOUBLE QUOTED STRING"] = {
		id = 12,
		fgColor = rgb(0x7F0000),
		bgColor = rgb(0xFFFFFF),
	},
	["ESCAPE SEQUENCE"] = {
		id = 13,
		fgColor = rgb(0x0000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["DATE TIME"] = {
		id = 14,
		fgColor = rgb(0x007F7F),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING EOL"] = {
		id = 15,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xE0C0E0),
	},
}

return L
