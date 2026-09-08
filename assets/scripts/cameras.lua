local cam1
local cam2

function start()
    cam1 = find.obj("camera")
    cam2 = find.obj("testcam")
end

function update(deltaTime)
    if input.keyPressed(key.Q) then
        cam2:setCurrent()
    end
    if input.keyPressed(key.E) then
        cam1:setCurrent()
    end
    if input.keyPressed(key.T) then
        swapScene("assets/scenes/scene1.json")
    end
end