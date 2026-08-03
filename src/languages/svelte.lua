local L = {}

L.lexer = "hypertext"

L.extensions = {
	"svelte",
}

L.properties = {
	["fold.html"] = "1",
}

L.styles = {
	["DEFAULT"] = {
		id = 0,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["COMMENT"] = {
		id = 9,
		fgColor = rgb(0x008000),
		bgColor = rgb(0xFFFFFF),
	},
	["TAG"] = {
		id = 1,
		fgColor = rgb(0x0000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["ATTRIBUTE"] = {
		id = 3,
		fgColor = rgb(0xFF0000),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING"] = {
		id = 6,
		fgColor = rgb(0x8000FF),
		bgColor = rgb(0xFFFFFF),
	},
}

return L
