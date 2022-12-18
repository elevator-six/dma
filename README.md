[download this and extract it in the same directory](https://send.tresorit.com/a#lAzfTUbbQMejLIlA26JUfA)

# TODO
Clean up cod_dma.cpp; it is really messy and I hate it. But there's a lot to break in there so GG to myself when I do that.

# Changelog for mr matt
- Cleaned up most of the files and deleted the math files. They weren't being used.
- Added a pattern scan nested class to the memory class for future use.
- Added compatibility for using the get_bone_index decrypt. The umul128 function mainly. This adds the ability to render skeleton or get the right bone position when we're ready for that
- Adding on to the previous, I've added the get_bone_position function to be used later. It should work I just added it for when I want to add it.

Notes:
  The new umul function should fix the weird glitch we saw with the heads.
