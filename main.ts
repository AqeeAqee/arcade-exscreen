game.stats=true

scene.setBackgroundColor(7)
tiles.setCurrentTilemap(tilemap`level2`)
let mySprite = sprites.create(img`
    . . . . . . f f f f . . . . . .
    . . . . f f f 2 2 f f f . . . .
    . . . f f f 2 2 2 2 f f f . . .
    . . f f f e e e e e e f f f . .
    . . f f e 2 2 2 2 2 2 e e f . .
    . . f e 2 f f f f f f 2 e f . .
    . . f f f f e e e e f f f f . .
    . f f e f b f 4 4 f b f e f f .
    . f e e 4 1 f d d f 1 4 e e f .
    . . f e e d d d d d d e e f . .
    . . . f e e 4 4 4 4 e e f . . .
    . . e 4 f 2 2 2 2 2 2 f 4 e . .
    . . 4 d f 2 2 2 2 2 2 f d 4 . .
    . . 4 4 f 4 4 5 5 4 4 f 4 4 . .
    . . . . . f f f f f f . . . . .
    . . . . . f f . . f f . . . . .
`, SpriteKind.Player)
controller.moveSprite(mySprite)
scene.cameraFollowSprite(mySprite)

extraScreen.init()
extraScreen.DisplayOnInnerScreen=true

controller.A.onEvent(ControllerButtonEvent.Pressed, function () {
    extraScreen.DisplayOnExtraScreen = !extraScreen.DisplayOnExtraScreen
})
controller.B.onEvent(ControllerButtonEvent.Pressed, function () {
    extraScreen.DisplayOnInnerScreen = !extraScreen.DisplayOnInnerScreen
})

// const buffer = Buffer.create(1000)
// const ms = control.micros()
// for (let i = 0; i < 1000; i++) {
//     //0.015ms
//     buffer.write(0, palette.getCurrentColors().buf)
// }
// info.setScore(control.micros() - ms)

// const testBuf = Buffer.create(120)
// const msTest = control.micros()
// //test, 27.185ms
// for (let i = 0; i < 160; i++) {
//     pins.spiTransfer(testBuf, null)
// }
// info.setLife(control.micros() - msTest)

// const colorbuf = Buffer.create(48)
// for (let i = 0; i < 48;) {
//     colorbuf.setUint8(i++, 0x04 * i)
//     colorbuf.setUint8(i++, 0x04 * i)
//     colorbuf.setUint8(i++, 0x04 * i)
//     pins.spiTransfer(colorbuf, null)
// }

// palette.setColors(color.ColorBuffer.fromBuffer(colorbuf,color.ColorBufferLayout.RGB))

/*
controller.A.onEvent(ControllerButtonEvent.Pressed, function () {
    screen.setBrightness(screen.brightness() + 5)
    info.setLife(screen.brightness())
})
controller.B.onEvent(ControllerButtonEvent.Pressed, function () {
    screen.setBrightness(screen.brightness() - 5)
    info.setLife(screen.brightness())
})
*/