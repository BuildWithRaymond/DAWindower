# Icon fixtures

These original test icons contain solid opaque colors at 16×16 and 32×32:

- `window_icon.ico`: RGB(19,155,208), resource group 101.
- `secondary_icon.ico`: RGB(226,69,85), resource group 201.

The native test compares the first group against Windows `ExtractIconA` index
zero and checks that the other group is not selected. Both sizes use 32-bit
BGRA DIB pixels with an empty AND mask. No game artwork is included. These files
are linked only into the test executable and never installed with either DLL.
