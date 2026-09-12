local fps
local frames = 0

function start()
    fps = find.ui("fps")
end

function update(deltaTime)
    frames = frames + 1

    if frames % 30 == 0 then
        fps.text = "FPS: " .. math.floor(1/deltaTime)
    end
end