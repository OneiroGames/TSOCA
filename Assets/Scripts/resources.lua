--
-- Copyright (c) Oneiro Games. All rights reserved.
-- Licensed under the GNU General Public License, Version 3.0.
--

-- DEFINE DIRECTORIES
local ASSETS_DIR <const> = "Assets/"

local IMAGES_DIR <const> = ASSETS_DIR .. "Images/"
local AUDIO_DIR <const> = ASSETS_DIR .. "Audio/"

local SPRITES_DIR <const> = IMAGES_DIR .. "Sprites/"
local BACKGROUNDS_DIR <const> = IMAGES_DIR .. "BGs/"
local CGS_DIR <const> = IMAGES_DIR .. "CGs/"
local UI_DIR <const> = IMAGES_DIR .. "UI/"

local MUSIC_DIR <const> = AUDIO_DIR .. "Music/"

-- DEFINE CHARACTERS
local textColor <const> = vec4(0.0, 0.0, 0.0, 1.0)

narrator = oneiro.Character("", vec4(0.0), textColor)
carl = oneiro.Character("[/i]Карл[/i]", vec4(0.27, 0.08, 0.1, 1.0), textColor)
tatiana = oneiro.Character("[/i]Татьяна[/i]", vec4(0.5, 0.0, 0.5, 1.0), textColor)
robin = oneiro.Character("[/i]Старик Робин[/i]", vec4(0.25, 0.19, 0.5, 1.0), textColor)
pacient = oneiro.Character("[/i]Пациент[/i]", vec4(1.0), textColor)
martin = oneiro.Character("[/i]Мартин[/i]", vec4(0.81, 0.29, 0.19, 1.0), textColor)

-- ATTACH SPRITES TO CHARACTERS
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_mask_neutral.png", true, "mask_neutral")
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_mask_angry.png", true, "mask_angry")
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_mask_cry.png", true, "mask_cry")
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_neutral.png", true, "neutral")
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_smile.png", true, "smile")
tatiana:attach(SPRITES_DIR .. "tatiana/tatiana_cry.png", true, "cry")

robin:attach(SPRITES_DIR .. "robin/robin_normal.png", true, "normal")

-- DEFINE BACKGROUNDS
doorToHome = oneiro.Background(BACKGROUNDS_DIR .. "door_to_home.jpg", false)
tatianaKitchen = oneiro.Background(BACKGROUNDS_DIR .. "tatiana_kitchen.jpg", false)
centerOffice = oneiro.Background(BACKGROUNDS_DIR .. "office.jpg", false)
dark = vec3()

-- DEFINE CGS
firstMeet = oneiro.Background(CGS_DIR .. "first_meet.jpg", false)

-- DEFINE AUDIO
mainTheme = oneiro.Music(MUSIC_DIR .. "main_theme.ogg")
tatianaTheme = oneiro.Music(MUSIC_DIR .. "tatiana_theme.ogg")
suffering = oneiro.Music(MUSIC_DIR .. "suffering.ogg")
therapy = oneiro.Music(MUSIC_DIR .. "therapy.ogg")

-- DEFINE TEXT BOXES
textbox = oneiro.TextBox(UI_DIR .. "textbox.png", vec2(), false, true)

-- DEFINE LABELS
start = oneiro.Label("start")
good = oneiro.Label("good")
bad = oneiro.Label("bad")