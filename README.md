# ez
An old fashioned display editor, ez.

*** default key-define table *** '84.11  NOTE:  ^:control e:escape (mk):marked

^I,HT:tab.ins.          CR,^M:line ins.         ^P:quoted ins.  ^O:open line

^H,BS:move left by one  ^K:move right 1ch.      ^U:prev. line   ^J,LF:next line

^A:head of line         ^L:tail of line         ^T,eV:prev.page ^V:next page

e<:head of text(mk)     e>:tail of text(mk)     e,:page top     e.:page tail

^P:mark                 e@:exchange mk/cursor   e^@:goto marker

^D:del one ch           DEL:1ch. before cursor  ^Z:del. to EOL  eZ:del. to TOL

eW:copy region(killbuf) ^W:save region & delete e^W:(pop out killb.)?

^Y:load from killb.(mk) ^N:recover deleted ch...

^F:search str./forward  ^B:search str./backward e(,e),e{,e}:balanced pair

eK:move to next word    eH:back to prev.word    eD:del. word    eDEL:pre.word

eC:initial to upper     eL:word to lower        eU:word to upper

--- file I/O and buffer operation ---

eS:save to file         eQ:save and exit        ^C:no-save exit

eI:load subfile         eO:write subfile        eR:rename file  e$:rename ddir

eF:switch/create buf.   eB:display buf.table    e^Z:del. buf.

e":def./end of macro    ^E:execute macro        e%:load key-def file

eM:(mail region)?       eP:(mail paragraph)?    ^R:repeat command

e?:command list(HELP)   e^X:system command exe.^G:refresh page

