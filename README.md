# _Deprecation Warning_

Please take a look at [KeyRemap4MacBook](https://pqrs.org/macosx/keyremap4macbook/). Its primary feature is key remapping; merging modifiers from different keyboards comes as a [side effect](https://pqrs.org/macosx/keyremap4macbook/document.html.en#implicit).

#### My Multiple Keyboards


![screenshot](https://github.com/mejedi/my-multiple-keyboards/raw/master/screenshot.png)

Do you have more than one keyboard attached to your Mac? Are you crazy
enough to use both simultaneously?  Or maybe you just own an assistive
device (say a foot pedal) that pretends to be another keyboard.  Anyway
there is one gotcha — on Mac each keyboard respects its own modifier
keys and ignores modifiers pressed on other keyboards.  This little
utility solves the issue in a user-friendly way.

Specifically System Preferences are extended with ‘My Multiple
Keyboards’ custom preference pane enabling to customize the processing
of modifier keys.  It is just that simple — run the installer; change
the settings the way you like it.  Settings are per-user and modifier
keys customization is turned off by default.

#### Install
Running *build.sh* will produce the installer package (MyMultipleKeyboars.pkg).
This obviously requires XCode.

This work is actually the complete rewrite of the very similar alterkeys utility by [Chance Miller](http://dotdotcomorg.net/Mac/). Mine has fewer bugs in it.

