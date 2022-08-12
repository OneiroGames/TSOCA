-- DEFINE DIRECTORIES
local IMAGES_DIR <const> = "Assets/Images/"
local BACKGROUNDS_DIR <const> = IMAGES_DIR .. "Backgrounds/"
local SPRITES_DIR <const> = IMAGES_DIR .. "Sprites/"

-- DEFINE CHARACTERS
narrator = oneiro.Character("")

-- ATTACH SPRITES TO CHARACTERS
narrator:attach(SPRITES_DIR .. "test.png", true, "test")

-- DEFINE BACKGROUNDS
testBg = oneiro.Background(BACKGROUNDS_DIR .. "test.png", false)

-- DEFINE LABELS
start = oneiro.Label("start")

-- DEFINE TEXT BOX
textbox = oneiro.TextBox("Assets/Images/UI/textbox.png", 0.0, 0.0, false)