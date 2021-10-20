# IsoMateric

When looking at the code take in consideration that this is my first project and I am a noob when it comes to programming.

It only builds on linux at the moment, and relative paths don't work, as you can see when trying to load shader files.

### TO-DO:
- [x] ~~Figure out why nothing renders below a certain point in the window~~
The frustrum's near parameter didn't include anything that was renderer before the 0.1 coordinate in the z-axis, so as a temporary fix, it was changed from 0.1 to -100 and now it does.
