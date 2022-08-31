--
-- Copyright (c) Oneiro Games. All rights reserved.
-- Licensed under the GNU General Public License, Version 3.0.
--

-- FUNCTION WRAPPERS
function scene(background, speed)
	if (speed)
	then
		oneiro.scene(background, speed)
	else
		oneiro.scene(background)
	end
end

function wait(time)
	oneiro.wait(time)
end

function changeTextBox(textBox)
	oneiro.changeTextBox(textBox)
end

function vec4(x, y, z, w)
	return oneiro.vec4(x, y, z, w)
end

function vec4(xyzw)
	return oneiro.vec4(xyzw)
end

function vec4()
	return oneiro.vec4()
end

function vec3(x, y, z)
	return oneiro.vec3(x, y, z)
end

function vec3(xyz)
	return oneiro.vec3(xyz)
end

function vec3()
	return oneiro.vec3()
end

function vec2(x, y)
	return oneiro.vec2(x, y)
end

function vec2(xy)
	return oneiro.vec2(xy)
end

function vec2()
	return oneiro.vec2()
end