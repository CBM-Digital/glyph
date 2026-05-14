package com.glyph.runtime;

import org.libsdl.app.SDLActivity;

public class GlyphActivity extends SDLActivity {
  @Override
  protected String[] getLibraries() {
    return new String[] {"SDL2", "glyph"};
  }
}
