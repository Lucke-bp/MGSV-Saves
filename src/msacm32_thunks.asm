; msacm32.dll proxy: one thunk per export of the real System32 msacm32.dll (same names and ordinals).
; A thunk jumps to the real function; the first call of each resolves it (msacm32_resolve, in C++).
; Generated list: keep in the order of k_msacm32_names in proxy_msacm32.cpp.

EXTERN msacm32_resolve:PROC

.data
PUBLIC msacm32_procs
msacm32_procs QWORD 42 DUP(0)

.code

; r10d: export index. Saves the argument registers, resolves, restores them and jumps (MMSYSERR_ERROR if the
; real DLL is missing).
msacm32_slow PROC FRAME
    push rcx
    .pushreg rcx
    push rdx
    .pushreg rdx
    push r8
    .pushreg r8
    push r9
    .pushreg r9
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov ecx, r10d
    call msacm32_resolve
    add rsp, 28h
    pop r9
    pop r8
    pop rdx
    pop rcx
    test rax, rax
    jz missing
    jmp rax
missing:
    mov eax, 1
    ret
msacm32_slow ENDP

THUNK MACRO name, index
    LOCAL slow
name PROC
    mov rax, QWORD PTR [msacm32_procs + index * 8]
    test rax, rax
    jz slow
    jmp rax
slow:
    mov r10d, index
    jmp msacm32_slow
name ENDP
ENDM

THUNK proxy_acmDriverAddA, 0
THUNK proxy_acmDriverAddW, 1
THUNK proxy_acmDriverClose, 2
THUNK proxy_acmDriverDetailsA, 3
THUNK proxy_acmDriverDetailsW, 4
THUNK proxy_acmDriverEnum, 5
THUNK proxy_acmDriverID, 6
THUNK proxy_acmDriverMessage, 7
THUNK proxy_acmDriverOpen, 8
THUNK proxy_acmDriverPriority, 9
THUNK proxy_acmDriverRemove, 10
THUNK proxy_acmFilterChooseA, 11
THUNK proxy_acmFilterChooseW, 12
THUNK proxy_acmFilterDetailsA, 13
THUNK proxy_acmFilterDetailsW, 14
THUNK proxy_acmFilterEnumA, 15
THUNK proxy_acmFilterEnumW, 16
THUNK proxy_acmFilterTagDetailsA, 17
THUNK proxy_acmFilterTagDetailsW, 18
THUNK proxy_acmFilterTagEnumA, 19
THUNK proxy_acmFilterTagEnumW, 20
THUNK proxy_acmFormatChooseA, 21
THUNK proxy_acmFormatChooseW, 22
THUNK proxy_acmFormatDetailsA, 23
THUNK proxy_acmFormatDetailsW, 24
THUNK proxy_acmFormatEnumA, 25
THUNK proxy_acmFormatEnumW, 26
THUNK proxy_acmFormatSuggest, 27
THUNK proxy_acmFormatTagDetailsA, 28
THUNK proxy_acmFormatTagDetailsW, 29
THUNK proxy_acmFormatTagEnumA, 30
THUNK proxy_acmFormatTagEnumW, 31
THUNK proxy_acmGetVersion, 32
THUNK proxy_acmMetrics, 33
THUNK proxy_acmStreamClose, 34
THUNK proxy_acmStreamConvert, 35
THUNK proxy_acmStreamMessage, 36
THUNK proxy_acmStreamOpen, 37
THUNK proxy_acmStreamPrepareHeader, 38
THUNK proxy_acmStreamReset, 39
THUNK proxy_acmStreamSize, 40
THUNK proxy_acmStreamUnprepareHeader, 41

END
