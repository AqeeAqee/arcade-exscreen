/**
 * aqee, 2022-1:
 * !!! use pins.SCL instead pins.P19 on meowbit !!!
 *  auto update pallete, send to extra Screen each frame
 *  set brightness with existed screen.setBrightness(), auto send to extra screen
 *  compatible with my 3DRaycasting ext, by draw tempScreen to Screen in Raycasting Ext., instead of override updateScreen()
 * TODO
 * ? reduce memory using, by send screen with small buffer, 1/10?
 * fill black when turning off inner/extra screen
 * Package sync issue with esp.
 * Issue:
 *   0% error on Sokoban
 *   021 or 0% error when open menu with innerScreen turned off.
 */


//% color="#275C6B" icon="\uf26c" weight=95 block="ExtraScreen"
namespace extraScreen {
    export let DisplayOnExtraScreen = true
    export let DisplayOnInnerScreen = false

    // Display constants
    export const TFTWIDTH = 120 // ? can't get screen content beyond 120
    export const TFTHEIGHT = 160
    const BUFFER_SIZE_SCREEN = TFTWIDTH * TFTHEIGHT
    const BUFFER_SIZE_PALLETE = 48
    const BUFFER_SIZE_STATUS = 20
    const BUFFER_SIZE_BRIGHTNESS = 1
    const OFFSET_PALLETE = BUFFER_SIZE_SCREEN
    const OFFSET_STATUS = OFFSET_PALLETE + BUFFER_SIZE_PALLETE
    const OFFSET_BRIGHTNESS = OFFSET_STATUS + BUFFER_SIZE_STATUS
    const buf = control.createBuffer(BUFFER_SIZE_SCREEN + BUFFER_SIZE_PALLETE + BUFFER_SIZE_STATUS + BUFFER_SIZE_BRIGHTNESS + 3) //aligned to 4-byte
    buf.setUint8(OFFSET_BRIGHTNESS + 1, 0x77)
    buf.setUint8(OFFSET_BRIGHTNESS + 2, 0x07)
    buf.setUint8(OFFSET_BRIGHTNESS + 3, 0x05)

    let lastSendScreenTime = 0

    //debug
    let ms = 0, msStart = 0, count = 0

    let colors16bit: number[]
    function prepareArcadeColors() {
        const hexChar = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F",]
        colors16bit = []
        let colors = palette.getCurrentColors()
        for (let i = 0; i < 16; i++) {
            const c = colors.color(i)
            colors16bit.push(((c & 0xF80000) >> 8) | ((c & 0x00FC00) >> 5) | ((c & 0x0000F8) >> 3))
            let bits = ""
            // 0B...
            // for(let b=0x8000;b>0;b>>=1)
            //     bits += (colors16bit[i]&b)?"1":"0"
            // 0X...
            for (let b = 12; b >= 0; b -= 4)
                bits += hexChar[(colors16bit[i] >> b) & 0x0F]
            console.log(bits)
        }
        // console.log(colors16bit)
    }
    // prepareArcadeColors()
    /** results:
    0  0000 0000000000000000
    1  FFFF 1111111111111111
    2  F904 1111100100000100
    3  FC98 1111110010011000
    4  FC06 1111110000000110
    5  FFA1 1111111110100001
    6  24F4 0010010011110100
    7  7EEA 0111111011101010
    8  01F5 0000000111110101
    9  879F 1000011110011111
    10 8978 1000100101111000
    11 A413 1010010000010011
    12 5A0D 0101101000001101
    13 E678 1110011001111000
    14 9227 1001001000100111
    15 0000 0000000000000000
     */
    //0,65535,63748,64664,64518,65441,9460,32490,501,34719,35192,42003,23053,59000,37415,0

    let statUpdated = false
    const statBuf = control.createBuffer(160 * 8)
    let statusMsg = ""

    //clone arcade screen to lcd
    export function cloneScreen_ColorIndex() {
        // //debug
        if (lastSendScreenTime + 31 - control.millis() > 0) {
            screen.drawLine(0, 0, lastSendScreenTime + 31 - control.millis(), 0, 2)
        }
        // if (msStart == 0) msStart = control.micros()
        // count++
        // ms = control.micros()
        // info.setScore(control.micros() - ms)

        //pallete, 17micros
        buf.write(OFFSET_PALLETE, palette.getCurrentColors().buf)

        //status
        if (statUpdated) {
            {//method 1, send out text, 70micros
            let i = 0
            for(;i<statusMsg.length;i++)
                buf.setUint8(OFFSET_STATUS+i, statusMsg.charCodeAt(i) )
            for (; i < 20; i++)
                buf.setUint8(OFFSET_STATUS + i, 0x20)
            }
            // { //method 2, print on screen, 660micros. Screen stained.
            // screen.print(statusMsg, 2, 120-8+2,1,image.font5) //print inside 120
            // }
            
            statUpdated=false
        }

        //brightness
        buf.setUint8(OFFSET_BRIGHTNESS, screen.brightness())

        //screen, about 1.5ms
        screen.getRows(0, buf)

        pause(lastSendScreenTime + 31 - control.millis()) // min interal = transfer time to extra screen = (240*320*16)/40M= 30.72ms

        lastSendScreenTime = control.millis()
        // csPin.digitalWrite(false)  //commented to avoid 0-size package
        // about 7.5ms
        pins.spiTransfer(buf, null)
        // csPin.digitalWrite(true)  //commented to avoid 0-size package
        // info.setLife(count * 1000 * 1000 * 1000 / (control.micros() - msStart))

    }

    export function init() {
        pins.spiMode(0)
        // pins.spiFrequency(25000000)
        // pins.spiWrite(0x77) //write any thing to init spi, or lcd can init failed.
        // control.waitMicros(1000*1000)

        // if(csPin){
        //     if (control.ramSize() > 1024 * 1024 * 16) {
        //     //simulator, walk around pins issue, it doesn't kown some pins
        //         csPin = pins.P0
        //         scene.backgroundImage().printCenter("simulator", 0, 2)
        //     }else
        //         csPin.digitalWrite(false)
        // }

        //% shim=pxt::updateScreen
        function updateScreen(img: Image) { }
        control.__screen.setupUpdate(() => {
            let ms1 = control.micros()
            if (DisplayOnExtraScreen)
                extraScreen.cloneScreen_ColorIndex()
            ms1 = control.micros() - ms1
            info.setScore(ms1)

            // let ms2 = control.micros()
            if (DisplayOnInnerScreen)
                updateScreen(screen) //13.7ms, 30->42.1fps if skip.
            // ms2 = control.micros() - ms2

            // console.log([ms1,ms2].join())
        })

        //% shim=pxt::updateScreenStatusBar
        function updateScreenStatusBar(img: Image): void { return }
        const onStats_origin = control.EventContext.onStats
        control.EventContext.onStats = function (msg: string) {
            statusMsg = msg.slice(0, 20)
            statUpdated = true
            onStats_origin(msg)
        }
    }
}

