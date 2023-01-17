
pins.spiMode(2)
const buffer = Buffer.create(1000)
const ms = control.micros()
for (let i = 0; i < 1000; i++) {
    //0.015ms
    buffer.write(0, palette.getCurrentColors().buf)
}
info.setScore(control.micros() - ms)

//test, 27.185ms
const testBuf = Buffer.create(120)
const msTest = control.micros()
for (let i = 0; i < 160; i++) {
    pins.spiTransfer(testBuf, null)
}
info.setLife(control.micros() - msTest)

const colorbuf = Buffer.create(48)
for (let i = 0; i < 48;) {
    colorbuf.setUint8(i++, 0x04 * i)
    colorbuf.setUint8(i++, 0x04 * i)
    colorbuf.setUint8(i++, 0x04 * i)
    pins.spiTransfer(colorbuf, null)
}

// palette.setColors(color.ColorBuffer.fromBuffer(colorbuf,color.ColorBufferLayout.RGB))

extraScreen.init()


controller.A.onEvent(ControllerButtonEvent.Pressed, function () {
    extraScreen.DisplayOnExtraScreen = !extraScreen.DisplayOnExtraScreen
})
controller.B.onEvent(ControllerButtonEvent.Pressed, function () {
    extraScreen.DisplayOnInnerScreen = !extraScreen.DisplayOnInnerScreen
})
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