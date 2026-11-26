; =============================================================================
; Minimal clean-room PC-8801mkIISR Main IPL Boot ROM (N88.ROM stub)
; Loaded at 0x0000..0x7FFF (with ROM_VERSION '8' placed at 0x79D7).
; Initializes 8255 PPI, CRTC/DMAC 80x25 color, reads Drive 0 Track 0 Sector 1
; into 0xC000..0xC0FF via Sub-CPU commands 0x02 + 0x03, and jumps to 0xC000.
; =============================================================================
        .module n88_stub
        .area   _ROM (ABS)
        .org    0x0000

_reset:
        di
        ld      sp, #0xBFF0

        ; Basic display/memory control: keep Main ROM at 0x0000..0x7FFF (bit1=0)
        ld      a, #0x31
        out     (0x30), a       ; 80-col color
        ld      a, #0x39
        out     (0x31), a       ; 200-line, color, VDISP=1, 25-line, ROM=on
        ld      a, #0xA0
        out     (0x32), a       ; SINTM=1, EVRAM=0, ANALOG=1, TMODE=0
        xor     a
        out     (0x35), a       ; GAM=0
        out     (0x53), a       ; Show TEXT + B + R + G
        out     (0x5F), a       ; Main RAM at 0xC000..0xEFFF

        ; Initialize Main 8255 PPI: Mode 0, PA=in, PB=out, PCH=out, PCL=in
        ld      a, #0x91
        out     (0xFF), a
        xor     a
        out     (0xFE), a

        ; Initialize CRTC 80x25 color + DMAC ch2 (TVRAM at 0xF3C8)
        xor     a
        out     (0x51), a       ; CRTC RESET
        ld      a, #0xCE
        out     (0x50), a
        ld      a, #0x98
        out     (0x50), a
        ld      a, #0x6F
        out     (0x50), a
        ld      a, #0x58
        out     (0x50), a
        ld      a, #0x53
        out     (0x50), a
        ld      a, #0x80
        out     (0x51), a       ; Cursor off
        xor     a
        out     (0x50), a
        out     (0x50), a

        out     (0x68), a
        ld      a, #0xC8
        out     (0x64), a
        ld      a, #0xF3
        out     (0x64), a
        ld      a, #0xB7
        out     (0x65), a
        ld      a, #0x4B
        out     (0x65), a
        ld      a, #0xE4
        out     (0x68), a

        ld      a, #0x20        ; CRTC START DISPLAY
        out     (0x51), a
        ld      a, #0x43        ; CRTC INT MASK = 3
        out     (0x51), a

        ; Issue Sub-CPU 0x00 (Initialize)
        xor     a
        call    _putcom

        ; Issue Sub-CPU 0x02 (Read Data): [0x02, count=1, drv=0, trk=0, sec=1]
        ld      a, #0x02
        call    _putcom
        ld      a, #0x01        ; count = 1 sector
        call    _putdat
        xor     a               ; drive = 0
        call    _putdat
        xor     a               ; track = 0
        call    _putdat
        ld      a, #0x01        ; sector = 1
        call    _putdat

        ; Issue Sub-CPU 0x03 (Send Data) and read 256 bytes into 0xC000..0xC0FF
        ld      a, #0x03
        call    _putcom
        ld      hl, #0xC000
        ld      b, #0           ; 256 bytes
_rx_ipl:
        call    _getdat
        ld      (hl), a
        inc     hl
        djnz    _rx_ipl

        ; Boot loaded IPL at 0xC000!
        jp      0xC000

; --- 8255 PPI Main-side handshake (identical protocol to PC-88 BIOS) ---
_putcom:
        push    af
        ld      a, #0x0F        ; ATN = 1
        out     (0xFF), a
_w_rfd1:
        in      a, (0xFE)
        and     #0x06           ; Wait Sub RFD=1 (bit1) & DAC=0 (bit2)
        cp      #0x02
        jr      nz, _w_rfd1
        ld      a, #0x0E        ; ATN = 0
        out     (0xFF), a
        pop     af
_putdat:
        push    af
_w_rfd2:
        in      a, (0xFE)
        and     #0x06
        cp      #0x02
        jr      nz, _w_rfd2
        pop     af
        out     (0xFD), a       ; Port B data out
        ld      a, #0x09        ; DAV = 1
        out     (0xFF), a
_w_dac1:
        in      a, (0xFE)
        and     #0x04           ; Wait Sub DAC=1 (bit2)
        jr      z, _w_dac1
        ld      a, #0x08        ; DAV = 0
        out     (0xFF), a
_w_dac0:
        in      a, (0xFE)
        and     #0x04           ; Wait Sub DAC=0
        jr      nz, _w_dac0
        ret

_getdat:
        ld      a, #0x0B        ; RFD = 1
        out     (0xFF), a
_w_dav1:
        in      a, (0xFE)
        and     #0x01           ; Wait Sub DAV=1 (bit0)
        jr      z, _w_dav1
        ld      a, #0x0A        ; RFD = 0
        out     (0xFF), a
        in      a, (0xFC)       ; Port A data in
        push    af
        ld      a, #0x0D        ; DAC = 1
        out     (0xFF), a
_w_dav0:
        in      a, (0xFE)
        and     #0x01           ; Wait Sub DAV=0
        jr      nz, _w_dav0
        ld      a, #0x0C        ; DAC = 0
        out     (0xFF), a
        pop     af
        ret
