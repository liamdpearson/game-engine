local camnhands
local crosshair
local walking
local ads

function start()
    camnhands = find.obj("camnhands")
    camnhands.rig:setAnim(0)
    crosshair = find.ui("crosshair")
    print(crosshair)
    walking = false
    ads = false
end

function update(deltaTime)
    self.transform.yaw = self.transform.yaw - input.mouseDX() * 0.05
    camnhands.transform.pitch = camnhands.transform.pitch - input.mouseDY() * 0.05

    acceleration = 30.0 * deltaTime

    moveDir = Vec3.new(0)

    forward = Vec3.new(
        -math.sin(math.rad(self.transform.yaw)),
        0.0,
        -math.cos(math.rad(self.transform.yaw))
    )

    right = Vec3.new(
        -math.sin(math.rad(self.transform.yaw + 90.0)),
        0.0,
        -math.cos(math.rad(self.transform.yaw + 90.0))
    )

    if input.keyHeld(key.W) then
        moveDir = moveDir + forward
    end
    if input.keyHeld(key.A) then
        moveDir = moveDir + right
    end
    if input.keyHeld(key.S) then
        moveDir = moveDir - forward
    end
    if input.keyHeld(key.D) then
        moveDir = moveDir - right
    end
    if input.keyHeld(key.LSHIFT) then
        acceleration = acceleration * 1.5
    end
    if input.keyPressed(key.SPACE) and self.grounded then
        self.velocity.y = self.velocity.y + 5.0
    end

    moveDirLen = moveDir:length()
    if (moveDirLen > 0.0) then
        moveDir:normalize()
        camnhands.rig.animSpeed = 1.5
        walking = true
    else
        camnhands.rig.animSpeed = 1.0
        walking = false
    end

    self.velocity.y = self.velocity.y - 9.81 * deltaTime
    if (self.velocity.y < -50.0) then
        self.velocity.y = -50.0
    end

    damping = 0.00005 ^ deltaTime
    self.velocity.x = self.velocity.x * damping
    self.velocity.z = self.velocity.z * damping
    self.velocity = self.velocity + moveDir * acceleration

    self:resolveCollisions(deltaTime)

    if input.mousePressed(mouse.RIGHT) then
        ads = true
        crosshair.draw = false
    end
    if input.mouseReleased(mouse.RIGHT) then
        ads = false
        crosshair.draw = true
    end

    w = 0
    if walking then
        w = 1
    end
    a = 0
    if ads then
        a = 1
    end
    shouldBe = w + a * 2
    if camnhands.rig.currentAnim ~= shouldBe then
        camnhands.rig:setAnim(shouldBe, 0.1)
    end

end