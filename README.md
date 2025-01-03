# usage instructions

## getting a ready release

1. go to the releases tab (.../atmosim/releases)
2. get the latest release depending on your system

## compiling it yourself (will run faster)

just make sure you have gcc/g++ ready and run build.bat or build_linux.sh depending on your OS (both *should* work for linux, though)

## running

1. open a console (windows: win+r->cmd, linux: you should know), then `cd path/to/atmosim`
2. run `atmosim -h` or `atmosim.exe -h`
3. read the help window, also read this README
4. enjoy

## using

### what is "mix" and "third"?

atmosim normally assumes your bomb is a mix of 2 gases in a tank (the ones you hold in hand) along with a third gas in a canister (the large ones you drag around), though it also provides you the values you should use if you decide for it to be vice versa (mix in canister, third gas in tank)<br>
"mix" will refer to the aforementioned mix of gases, while "third" will refer to the aforementioned third gas

### what is "temp1" and temp2"?

temp1 and temp2 refer to the iteration bounds of what atmosim checks<br>
this means that if you give it a third gas temp1 of, say, 375.15K, and a temp2 of 593.15K, it will check bombs with third gas temperatures ranging from 375.15K to 593.15K<br>
if temp1 and temp2 are equal, atmosim will not iterate temperatures for that gas/gas mix<br>
note that if temp1 and temp2 are different for both the mix and the third gas, atmosim may take a long time to compute, especially with -s

### what are useful arguments atmosim has?

-s: usually makes atmosim take much longer to do its job, but has a very good chance of giving you better results if you're making a bomb optimised for radius

--param: lets you optimise bombs for things that aren't its radius, like making it take longer to explode or minimising its tritium consumption

--radius \<value>: to be used with --param, makes atmosim never output bombs below the specified radius<br>
example: --radius20

--ticks \<value>: lets you set the ticks limit. allows you to either make atmosim run faster if you're making bombs that explode very fast, or, alternatively, allows you to make bombs with longer fuses. default: 30<br>
1s = 2tick

-m: mixing assistant tool, disables normal atmosim functionality. if you have 2 gases at different temperatures but want to get a specific moles1:moles2 (moles of first gas to moles of second gas) ratio, this lets you know what mixer settings to use<br>
when it asks you about heat capacity, providing an accurate number is optional. you may write "1" or any other positive number if you don't know. this only matters for having it let you know the temperature of the resulting mix
