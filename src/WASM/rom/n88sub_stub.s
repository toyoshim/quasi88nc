; =============================================================================
; Minimal clean-room PC-8801 FDD Sub-CPU ROM (N88SUB.ROM stub, <= 2KB)
; Runs on the PC-88 Sub-CPU Z80 (ROM 0x0000..0x1FFF, RAM 0x4000..0x7FFF).
; Implements 8255 PPI (0xFC..0xFF) + uPD765A FDC (0xF8/0xFA/0xFB) commands:
;   0x00 : Initialize (SPECIFY non-DMA)
;   0x02 : Read Data  [count, drv, trk, sec] -> Sub RAM 0x6000..
;   0x03 : Send Data  -> transmit count*256 bytes from 0x6000.. to Main
;   0x06 : Send Result Status -> transmit last ST0 error mask
; =============================================================================
        .module n88sub_stub
        .area   _ROM (ABS)
        .org    0x0000

; Sub RAM variables (0x5000..) & Sector buffer (0x6000..0x6FFF, 16 sectors)
        VAR_CNT  = 0x5000
        VAR_DRV  = 0x5001
        VAR_TRK  = 0x5002
        VAR_SEC  = 0x5003
        VAR_ST0  = 0x5004
        SEC_BUF  = 0x6000

_sub_reset:
        di
        im      1
        ld      sp, #0x7FFF
        jp      _sub_main

        .org    0x0038
_sub_irq:
        ret

_sub_main:
        ; Init Sub 8255 PPI: Mode 0, PA=in, PB=out, PCH=out, PCL=in
        ld      a, #0x91
        out     (0xFF), a
        xor     a
        out     (0xFE), a
        ld      (VAR_CNT), a
        ld      (VAR_ST0), a

        call    _fdc_specify

_cmd_loop:
        ; Wait for Main ATN = 1 (Port 0xFE bit 3)
        in      a, (0xFE)
        bit     3, a
        jr      z, _cmd_loop

        call    _sub_getdat     ; Receive command byte in A
        or      a
        jr      z, _cmd_init    ; 0x00: Initialize
        cp      #0x02
        jr      z, _cmd_read    ; 0x02: Read Data
        cp      #0x03
        jp      z, _cmd_send    ; 0x03: Send Data
        cp      #0x06
        jr      z, _cmd_stat    ; 0x06: Send Result Status
        jr      _cmd_loop

_cmd_init:
        call    _fdc_specify
        xor     a
        ld      (VAR_ST0), a
        jr      _cmd_loop

_cmd_stat:
        ld      a, (VAR_ST0)
        call    _sub_putdat
        jr      _cmd_loop

; --- Command 0x02: Read Data [count, drv, trk, sec] ---
_cmd_read:
        call    _sub_getdat
        ld      (VAR_CNT), a
        call    _sub_getdat
        and     #0x03
        ld      (VAR_DRV), a
        call    _sub_getdat
        ld      (VAR_TRK), a
        call    _sub_getdat
        ld      (VAR_SEC), a

        ld      hl, #SEC_BUF
        ld      a, (VAR_CNT)
        or      a
        jr      z, _cmd_loop
        ld      b, a            ; B = remaining sectors

_rd_sec_loop:
        push    bc
        push    hl

        ; Compute cylinder = trk >> 1, head = trk & 1, hd_us = (head << 2) | drv
        ld      a, (VAR_TRK)
        ld      c, a
        srl     a
        ld      d, a            ; D = cylinder (0..39)
        ld      a, c
        and     #0x01
        ld      e, a            ; E = head (0..1)
        rlca
        rlca
        ld      c, a
        ld      a, (VAR_DRV)
        or      c
        ld      c, a            ; C = hd_us = (head << 2) | drv

        ; Issue FDC SEEK: [0x0F, hd_us, cyl]
        ld      a, #0x0F
        call    _fdc_wr
        ld      a, c
        call    _fdc_wr
        ld      a, d
        call    _fdc_wr

        ; Poll SENSE INTERRUPT STATUS (0x08) until ST0 != 0x80
_wait_seek:
        ld      a, #0x08
        call    _fdc_wr
        call    _fdc_rd
        cp      #0x80
        jr      z, _wait_seek
        call    _fdc_rd         ; discard PCN

        ; Issue FDC READ DATA (0x46 MFM):
        ; [0x46, hd_us, C, H, R, N=1(256B), EOT=R, GPL=0x1B, DTL=0xFF]
        ld      a, #0x46
        call    _fdc_wr
        ld      a, c            ; hd_us
        call    _fdc_wr
        ld      a, d            ; C (cylinder)
        call    _fdc_wr
        ld      a, e            ; H (head)
        call    _fdc_wr
        ld      a, (VAR_SEC)    ; R (sector 1..16)
        call    _fdc_wr
        ld      a, #0x01        ; N = 1 (256 bytes/sector)
        call    _fdc_wr
        ld      a, (VAR_SEC)    ; EOT = R (read 1 sector)
        call    _fdc_wr
        ld      a, #0x1B        ; GPL
        call    _fdc_wr
        ld      a, #0xFF        ; DTL
        call    _fdc_wr

        ; Execution Phase: read up to 256 bytes into (HL++)
        pop     hl
        ld      c, #0           ; 256 bytes
_rd_exec:
        in      a, (0xFA)       ; FDC Main Status Register
        bit     7, a            ; RQM?
        jr      z, _rd_exec
        bit     5, a            ; NON_DMA (Execution phase)?
        jr      z, _rd_result   ; Dropped to Result phase early (error)
        in      a, (0xFB)       ; Read data byte
        ld      (hl), a
        inc     hl
        dec     c
        jr      nz, _rd_exec

        ; Pulse TC (Terminal Count) via IN (0xF8) to end E-phase cleanly
        in      a, (0xF8)

_rd_result:
        ; Read 7 Result Phase bytes: ST0, ST1, ST2, C, H, R, N
        call    _fdc_rd
        and     #0xC0           ; Keep IC bits (0x00 = Normal Termination)
        ld      (VAR_ST0), a
        ld      b, #6
_rd_res_lp:
        call    _fdc_rd
        djnz    _rd_res_lp

        ; Advance sector/track (16 sectors per 2D track)
        ld      a, (VAR_SEC)
        inc     a
        cp      #17
        jr      c, _sec_ok
        ld      a, #1
        ld      (VAR_SEC), a
        ld      a, (VAR_TRK)
        inc     a
        ld      (VAR_TRK), a
        jr      _sec_adv_done
_sec_ok:
        ld      (VAR_SEC), a
_sec_adv_done:
        pop     bc
        dec     b
        jp      nz, _rd_sec_loop
        jp      _cmd_loop

; --- Command 0x03: Send Data (VAR_CNT * 256 bytes from SEC_BUF) ---
_cmd_send:
        ld      hl, #SEC_BUF
        ld      a, (VAR_CNT)
        or      a
        jp      z, _cmd_loop
        ld      b, a
_snd_page:
        ld      c, #0           ; 256 bytes
_snd_byt:
        ld      a, (hl)
        inc     hl
        call    _sub_putdat
        dec     c
        jr      nz, _snd_byt
        djnz    _snd_page
        jp      _cmd_loop

; --- uPD765A FDC Helpers ---
_fdc_specify:
        ld      a, #0x03        ; SPECIFY
        call    _fdc_wr
        ld      a, #0xDF        ; SRT=13, HUT=15
        call    _fdc_wr
        ld      a, #0x03        ; HLT=1, ND=1 (non-DMA!)
        jp      _fdc_wr

_fdc_wr:
        push    af
_fw_wait:
        in      a, (0xFA)
        and     #0xC0           ; RQM=1 (bit7), DIO=0 (bit6)
        cp      #0x80
        jr      nz, _fw_wait
        pop     af
        out     (0xFB), a
        ret

_fdc_rd:
        in      a, (0xFA)
        and     #0xC0           ; RQM=1 (bit7), DIO=1 (bit6)
        cp      #0xC0
        jr      nz, _fdc_rd
        in      a, (0xFB)
        ret

; --- 8255 PPI Sub-side handshake ---
_sub_getdat:
        ld      a, #0x0B        ; Sub RFD = 1
        out     (0xFF), a
_sg_dav1:
        in      a, (0xFE)
        and     #0x01           ; Wait Main DAV = 1
        jr      z, _sg_dav1
        ld      a, #0x0A        ; Sub RFD = 0
        out     (0xFF), a
        in      a, (0xFC)       ; Read byte from Main
        push    af
        ld      a, #0x0D        ; Sub DAC = 1
        out     (0xFF), a
_sg_dav0:
        in      a, (0xFE)
        and     #0x01           ; Wait Main DAV = 0
        jr      nz, _sg_dav0
        ld      a, #0x0C        ; Sub DAC = 0
        out     (0xFF), a
        pop     af
        ret

_sub_putdat:
        push    af
_sp_rfd:
        in      a, (0xFE)
        and     #0x06           ; Wait Main RFD=1 (bit1) & DAC=0 (bit2)
        cp      #0x02
        jr      nz, _sp_rfd
        pop     af
        out     (0xFD), a       ; Write byte to Main
        ld      a, #0x09        ; Sub DAV = 1
        out     (0xFF), a
_sp_dac1:
        in      a, (0xFE)
        and     #0x04           ; Wait Main DAC = 1
        jr      z, _sp_dac1
        ld      a, #0x08        ; Sub DAV = 0
        out     (0xFF), a
_sp_dac0:
        in      a, (0xFE)
        and     #0x04           ; Wait Main DAC = 0
        jr      nz, _sp_dac0
        ret
