local L = {}

-- Lexilla does not ship a dedicated HCL lexer in this version. HCL uses the
-- C++ lexer as a structural fallback so braces, strings, numbers, and
-- operators remain useful until a native lexer is available.
L.lexer = "cpp"

L.singleLineComment = "# "

L.extensions = {
	"hcl",
}

L.keywords = {
	[0] = "bool false for if in null true",
	[1] = "any list map number object set string tuple",
}

L.styles = {
	["DEFAULT"] = {
		id = 11,
		fgColor = rgb(0x000000),
		bgColor = rgb(0xFFFFFF),
	},
	["COMMENT"] = {
		id = 1,
		fgColor = rgb(0x008000),
		bgColor = rgb(0xFFFFFF),
	},
	["NUMBER"] = {
		id = 4,
		fgColor = rgb(0xFF8000),
		bgColor = rgb(0xFFFFFF),
	},
	["STRING"] = {
		id = 6,
		fgColor = rgb(0x808080),
		bgColor = rgb(0xFFFFFF),
	},
	["KEYWORD"] = {
		id = 5,
		fgColor = rgb(0x0000FF),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
	["TYPE"] = {
		id = 16,
		fgColor = rgb(0x8000FF),
		bgColor = rgb(0xFFFFFF),
	},
	["OPERATOR"] = {
		id = 10,
		fgColor = rgb(0x000080),
		bgColor = rgb(0xFFFFFF),
		fontStyle = 1,
	},
}

return L
