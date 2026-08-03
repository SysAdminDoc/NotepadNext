local L = {}

L.lexer = "markdown"

L.disableFoldMargin = true

L.extensions = {
	"mdx",
}

L.styles = {
	["DEFAULT"] = {
		id = 0,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["STRONG"] = {
		id = 2,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["EMPHASIS"] = {
		id = 4,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 2,
	},
	["HEADER"] = {
		id = 6,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["LINK"] = {
		id = 18,
		fgColor = rgb(0x0000AA),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 4,
	},
	["CODE"] = {
		id = 19,
		fgColor = rgb(0x000088),
		bgColor = rgb(0xEEEEEE),
	},
}

return L
