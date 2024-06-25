# IsoMateric
![IsoMateric](https://github.com/mm4rk3t/IsoMateric/assets/45017214/9361f7b5-3e3a-451e-a41b-9f03846457ec)

When looking at the code take in consideration that this is my first project and I am a noob when it comes to programming.

It only builds on linux at the moment, and relative paths don't work, as you can see when trying to load shader files.

## Preview
![Uploading ezgif-6-3683a41669.gif…]()

### TO-DO:
- [x] ~~Figure out why nothing renders below a certain point in the window~~
The frustrum's near parameter didn't include anything that was renderer before the 0.1 coordinate in the z-axis, so as a temporary fix, it was changed from 0.1 to -100 and now it does.
