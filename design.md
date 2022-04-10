# Markdown

## Colors

- xterm
  - VT100: `\033[35;47m[magenta on white]\033[m`
  - Control Sequence
    | Code                     |  Effect   | Note                                       |
    | :----------------------- | :-------: | :----------------------------------------- |
    | CSI n m                  |    SGR    | ANSI color code (Select Graphic Rendition) |
    | CSI 38 ; 5 ; n m         | 256COLOR  | Foreground 256 color code                  |
    | ESC 48 ; 5 ; n m         | 256COLOR  | Background 256 color code                  |
    | CSI 38 ; 2 ; r ; g ; b m | TRUECOLOR | Foreground 24 bit rgb color code           |
    | ESC 48 ; 2 ; r ; g ; b m | TRUECOLOR | Background 24 bit rgb color code           |

- markdown
  - `<span style="color:magenta;background-color:white;">[magenta on white]</span>`
  - `<span style="color:#FF00FF;background-color:#FFFFFF;">[magenta on white]</span>`

## Styles

- xterm
  - https://tintin.mudhalla.net/info/xterm
  - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

- markdown
  - **bold**
    - __bold__
    - <strong>bold</strong>
  - faint
  - _italic_
    - *italic*
    - <i>italic</i>
    - <em>italic</em>
    - <span style="font-style:italic;">italic</span>
  - <u>underline</u>
    - <span style="text-decoration:underline;">underline</span>
  - blink
  - inverse
  - invisible
  - ~~crossed~~
    - <span style="text-decoration:line-through">crossed</span>


## Appendix

### Colors Table

| Name                 |                          HEX                          |                RGB |
| :------------------- | :---------------------------------------------------: | -----------------: |
| AliceBlue            | <span style="background-color:#F0F8FF">#F0F8FF</span> | RGB(240, 248, 255) |
| AntiqueWhite         | <span style="background-color:#FAEBD7">#FAEBD7</span> | RGB(250, 235, 215) |
| Aqua                 | <span style="background-color:#00FFFF">#00FFFF</span> |   RGB(0, 255, 255) |
| Aquamarine           | <span style="background-color:#7FFFD4">#7FFFD4</span> | RGB(127, 255, 212) |
| Azure                | <span style="background-color:#F0FFFF">#F0FFFF</span> | RGB(240, 255, 255) |
| Beige                | <span style="background-color:#F5F5DC">#F5F5DC</span> | RGB(245, 245, 220) |
| Bisque               | <span style="background-color:#FFE4C4">#FFE4C4</span> | RGB(255, 228, 196) |
| Black                | <span style="background-color:#000000">#000000</span> |       RGB(0, 0, 0) |
| BlanchedAlmond       | <span style="background-color:#FFEBCD">#FFEBCD</span> | RGB(255, 235, 205) |
| Blue                 | <span style="background-color:#0000FF">#0000FF</span> |     RGB(0, 0, 255) |
| BlueViolet           | <span style="background-color:#8A2BE2">#8A2BE2</span> |  RGB(138, 43, 226) |
| Brown                | <span style="background-color:#A52A2A">#A52A2A</span> |   RGB(165, 42, 42) |
| BurlyWood            | <span style="background-color:#DEB887">#DEB887</span> | RGB(222, 184, 135) |
| CadetBlue            | <span style="background-color:#5F9EA0">#5F9EA0</span> |  RGB(95, 158, 160) |
| Chartreuse           | <span style="background-color:#7FFF00">#7FFF00</span> |   RGB(127, 255, 0) |
| Chocolate            | <span style="background-color:#D2691E">#D2691E</span> |  RGB(210, 105, 30) |
| Coral                | <span style="background-color:#FF7F50">#FF7F50</span> |  RGB(255, 127, 80) |
| CornflowerBlue       | <span style="background-color:#6495ED">#6495ED</span> | RGB(100, 149, 237) |
| Cornsilk             | <span style="background-color:#FFF8DC">#FFF8DC</span> | RGB(255, 248, 220) |
| Crimson              | <span style="background-color:#DC143C">#DC143C</span> |   RGB(220, 20, 60) |
| Cyan                 | <span style="background-color:#00FFFF">#00FFFF</span> |   RGB(0, 255, 255) |
| DarkBlue             | <span style="background-color:#00008B">#00008B</span> |     RGB(0, 0, 139) |
| DarkCyan             | <span style="background-color:#008B8B">#008B8B</span> |   RGB(0, 139, 139) |
| DarkGoldenRod        | <span style="background-color:#B8860B">#B8860B</span> |  RGB(184, 134, 11) |
| DarkGray             | <span style="background-color:#A9A9A9">#A9A9A9</span> | RGB(169, 169, 169) |
| DarkGreen            | <span style="background-color:#006400">#006400</span> |     RGB(0, 100, 0) |
| DarkKhaki            | <span style="background-color:#BDB76B">#BDB76B</span> | RGB(189, 183, 107) |
| DarkMagenta          | <span style="background-color:#8B008B">#8B008B</span> |   RGB(139, 0, 139) |
| DarkOliveGreen       | <span style="background-color:#556B2F">#556B2F</span> |   RGB(85, 107, 47) |
| Darkorange           | <span style="background-color:#FF8C00">#FF8C00</span> |   RGB(255, 140, 0) |
| DarkOrchid           | <span style="background-color:#9932CC">#9932CC</span> |  RGB(153, 50, 204) |
| DarkRed              | <span style="background-color:#8B0000">#8B0000</span> |     RGB(139, 0, 0) |
| DarkSalmon           | <span style="background-color:#E9967A">#E9967A</span> | RGB(233, 150, 122) |
| DarkSeaGreen         | <span style="background-color:#8FBC8F">#8FBC8F</span> | RGB(143, 188, 143) |
| DarkSlateBlue        | <span style="background-color:#483D8B">#483D8B</span> |   RGB(72, 61, 139) |
| DarkSlateGray        | <span style="background-color:#2F4F4F">#2F4F4F</span> |    RGB(47, 79, 79) |
| DarkTurquoise        | <span style="background-color:#00CED1">#00CED1</span> |   RGB(0, 206, 209) |
| DarkViolet           | <span style="background-color:#9400D3">#9400D3</span> |   RGB(148, 0, 211) |
| DeepPink             | <span style="background-color:#FF1493">#FF1493</span> |  RGB(255, 20, 147) |
| DeepSkyBlue          | <span style="background-color:#00BFFF">#00BFFF</span> |   RGB(0, 191, 255) |
| DimGray              | <span style="background-color:#696969">#696969</span> | RGB(105, 105, 105) |
| DodgerBlue           | <span style="background-color:#1E90FF">#1E90FF</span> |  RGB(30, 144, 255) |
| Feldspar             | <span style="background-color:#D19275">#D19275</span> | RGB(209, 146, 117) |
| FireBrick            | <span style="background-color:#B22222">#B22222</span> |   RGB(178, 34, 34) |
| FloralWhite          | <span style="background-color:#FFFAF0">#FFFAF0</span> | RGB(255, 250, 240) |
| ForestGreen          | <span style="background-color:#228B22">#228B22</span> |   RGB(34, 139, 34) |
| Fuchsia              | <span style="background-color:#FF00FF">#FF00FF</span> |   RGB(255, 0, 255) |
| Gainsboro            | <span style="background-color:#DCDCDC">#DCDCDC</span> | RGB(220, 220, 220) |
| GhostWhite           | <span style="background-color:#F8F8FF">#F8F8FF</span> | RGB(248, 248, 255) |
| Gold                 | <span style="background-color:#FFD700">#FFD700</span> |   RGB(255, 215, 0) |
| GoldenRod            | <span style="background-color:#DAA520">#DAA520</span> |  RGB(218, 165, 32) |
| Gray                 | <span style="background-color:#808080">#808080</span> | RGB(128, 128, 128) |
| Green                | <span style="background-color:#008000">#008000</span> |     RGB(0, 128, 0) |
| GreenYellow          | <span style="background-color:#ADFF2F">#ADFF2F</span> |  RGB(173, 255, 47) |
| HoneyDew             | <span style="background-color:#F0FFF0">#F0FFF0</span> | RGB(240, 255, 240) |
| HotPink              | <span style="background-color:#FF69B4">#FF69B4</span> | RGB(255, 105, 180) |
| IndianRed            | <span style="background-color:#CD5C5C">#CD5C5C</span> |   RGB(205, 92, 92) |
| Indigo               | <span style="background-color:#4B0082">#4B0082</span> |    RGB(75, 0, 130) |
| Ivory                | <span style="background-color:#FFFFF0">#FFFFF0</span> | RGB(255, 255, 240) |
| Khaki                | <span style="background-color:#F0E68C">#F0E68C</span> | RGB(240, 230, 140) |
| Lavender             | <span style="background-color:#E6E6FA">#E6E6FA</span> | RGB(230, 230, 250) |
| LavenderBlush        | <span style="background-color:#FFF0F5">#FFF0F5</span> | RGB(255, 240, 245) |
| LawnGreen            | <span style="background-color:#7CFC00">#7CFC00</span> |   RGB(124, 252, 0) |
| LemonChiffon         | <span style="background-color:#FFFACD">#FFFACD</span> | RGB(255, 250, 205) |
| LightBlue            | <span style="background-color:#ADD8E6">#ADD8E6</span> | RGB(173, 216, 230) |
| LightCoral           | <span style="background-color:#F08080">#F08080</span> | RGB(240, 128, 128) |
| LightCyan            | <span style="background-color:#E0FFFF">#E0FFFF</span> | RGB(224, 255, 255) |
| LightGoldenRodYellow | <span style="background-color:#FAFAD2">#FAFAD2</span> | RGB(250, 250, 210) |
| LightGrey            | <span style="background-color:#D3D3D3">#D3D3D3</span> | RGB(211, 211, 211) |
| LightGreen           | <span style="background-color:#90EE90">#90EE90</span> | RGB(144, 238, 144) |
| LightPink            | <span style="background-color:#FFB6C1">#FFB6C1</span> | RGB(255, 182, 193) |
| LightSalmon          | <span style="background-color:#FFA07A">#FFA07A</span> | RGB(255, 160, 122) |
| LightSeaGreen        | <span style="background-color:#20B2AA">#20B2AA</span> |  RGB(32, 178, 170) |
| LightSkyBlue         | <span style="background-color:#87CEFA">#87CEFA</span> | RGB(135, 206, 250) |
| LightSlateBlue       | <span style="background-color:#8470FF">#8470FF</span> | RGB(132, 112, 255) |
| LightSlateGray       | <span style="background-color:#778899">#778899</span> | RGB(119, 136, 153) |
| LightSteelBlue       | <span style="background-color:#B0C4DE">#B0C4DE</span> | RGB(176, 196, 222) |
| LightYellow          | <span style="background-color:#FFFFE0">#FFFFE0</span> | RGB(255, 255, 224) |
| Lime                 | <span style="background-color:#00FF00">#00FF00</span> |     RGB(0, 255, 0) |
| LimeGreen            | <span style="background-color:#32CD32">#32CD32</span> |   RGB(50, 205, 50) |
| Linen                | <span style="background-color:#FAF0E6">#FAF0E6</span> | RGB(250, 240, 230) |
| Magenta              | <span style="background-color:#FF00FF">#FF00FF</span> |   RGB(255, 0, 255) |
| Maroon               | <span style="background-color:#800000">#800000</span> |     RGB(128, 0, 0) |
| MediumAquaMarine     | <span style="background-color:#66CDAA">#66CDAA</span> | RGB(102, 205, 170) |
| MediumBlue           | <span style="background-color:#0000CD">#0000CD</span> |     RGB(0, 0, 205) |
| MediumOrchid         | <span style="background-color:#BA55D3">#BA55D3</span> |  RGB(186, 85, 211) |
| MediumPurple         | <span style="background-color:#9370D8">#9370D8</span> | RGB(147, 112, 216) |
| MediumSeaGreen       | <span style="background-color:#3CB371">#3CB371</span> |  RGB(60, 179, 113) |
| MediumSlateBlue      | <span style="background-color:#7B68EE">#7B68EE</span> | RGB(123, 104, 238) |
| MediumSpringGreen    | <span style="background-color:#00FA9A">#00FA9A</span> |   RGB(0, 250, 154) |
| MediumTurquoise      | <span style="background-color:#48D1CC">#48D1CC</span> |  RGB(72, 209, 204) |
| MediumVioletRed      | <span style="background-color:#C71585">#C71585</span> |  RGB(199, 21, 133) |
| MidnightBlue         | <span style="background-color:#191970">#191970</span> |   RGB(25, 25, 112) |
| MintCream            | <span style="background-color:#F5FFFA">#F5FFFA</span> | RGB(245, 255, 250) |
| MistyRose            | <span style="background-color:#FFE4E1">#FFE4E1</span> | RGB(255, 228, 225) |
| Moccasin             | <span style="background-color:#FFE4B5">#FFE4B5</span> | RGB(255, 228, 181) |
| NavajoWhite          | <span style="background-color:#FFDEAD">#FFDEAD</span> | RGB(255, 222, 173) |
| Navy                 | <span style="background-color:#000080">#000080</span> |     RGB(0, 0, 128) |
| OldLace              | <span style="background-color:#FDF5E6">#FDF5E6</span> | RGB(253, 245, 230) |
| Olive                | <span style="background-color:#808000">#808000</span> |   RGB(128, 128, 0) |
| OliveDrab            | <span style="background-color:#6B8E23">#6B8E23</span> |  RGB(107, 142, 35) |
| Orange               | <span style="background-color:#FFA500">#FFA500</span> |   RGB(255, 165, 0) |
| OrangeRed            | <span style="background-color:#FF4500">#FF4500</span> |    RGB(255, 69, 0) |
| Orchid               | <span style="background-color:#DA70D6">#DA70D6</span> | RGB(218, 112, 214) |
| PaleGoldenRod        | <span style="background-color:#EEE8AA">#EEE8AA</span> | RGB(238, 232, 170) |
| PaleGreen            | <span style="background-color:#98FB98">#98FB98</span> | RGB(152, 251, 152) |
| PaleTurquoise        | <span style="background-color:#AFEEEE">#AFEEEE</span> | RGB(175, 238, 238) |
| PaleVioletRed        | <span style="background-color:#D87093">#D87093</span> | RGB(216, 112, 147) |
| PapayaWhip           | <span style="background-color:#FFEFD5">#FFEFD5</span> | RGB(255, 239, 213) |
| PeachPuff            | <span style="background-color:#FFDAB9">#FFDAB9</span> | RGB(255, 218, 185) |
| Peru                 | <span style="background-color:#CD853F">#CD853F</span> |  RGB(205, 133, 63) |
| Pink                 | <span style="background-color:#FFC0CB">#FFC0CB</span> | RGB(255, 192, 203) |
| Plum                 | <span style="background-color:#DDA0DD">#DDA0DD</span> | RGB(221, 160, 221) |
| PowderBlue           | <span style="background-color:#B0E0E6">#B0E0E6</span> | RGB(176, 224, 230) |
| Purple               | <span style="background-color:#800080">#800080</span> |   RGB(128, 0, 128) |
| Red                  | <span style="background-color:#FF0000">#FF0000</span> |     RGB(255, 0, 0) |
| RosyBrown            | <span style="background-color:#BC8F8F">#BC8F8F</span> | RGB(188, 143, 143) |
| RoyalBlue            | <span style="background-color:#4169E1">#4169E1</span> |  RGB(65, 105, 225) |
| SaddleBrown          | <span style="background-color:#8B4513">#8B4513</span> |   RGB(139, 69, 19) |
| Salmon               | <span style="background-color:#FA8072">#FA8072</span> | RGB(250, 128, 114) |
| SandyBrown           | <span style="background-color:#F4A460">#F4A460</span> |  RGB(244, 164, 96) |
| SeaGreen             | <span style="background-color:#2E8B57">#2E8B57</span> |   RGB(46, 139, 87) |
| SeaShell             | <span style="background-color:#FFF5EE">#FFF5EE</span> | RGB(255, 245, 238) |
| Sienna               | <span style="background-color:#A0522D">#A0522D</span> |   RGB(160, 82, 45) |
| Silver               | <span style="background-color:#C0C0C0">#C0C0C0</span> | RGB(192, 192, 192) |
| SkyBlue              | <span style="background-color:#87CEEB">#87CEEB</span> | RGB(135, 206, 235) |
| SlateBlue            | <span style="background-color:#6A5ACD">#6A5ACD</span> |  RGB(106, 90, 205) |
| SlateGray            | <span style="background-color:#708090">#708090</span> | RGB(112, 128, 144) |
| Snow                 | <span style="background-color:#FFFAFA">#FFFAFA</span> | RGB(255, 250, 250) |
| SpringGreen          | <span style="background-color:#00FF7F">#00FF7F</span> |   RGB(0, 255, 127) |
| SteelBlue            | <span style="background-color:#4682B4">#4682B4</span> |  RGB(70, 130, 180) |
| Tan                  | <span style="background-color:#D2B48C">#D2B48C</span> | RGB(210, 180, 140) |
| Teal                 | <span style="background-color:#008080">#008080</span> |   RGB(0, 128, 128) |
| Thistle              | <span style="background-color:#D8BFD8">#D8BFD8</span> | RGB(216, 191, 216) |
| Tomato               | <span style="background-color:#FF6347">#FF6347</span> |   RGB(255, 99, 71) |
| Turquoise            | <span style="background-color:#40E0D0">#40E0D0</span> |  RGB(64, 224, 208) |
| Violet               | <span style="background-color:#EE82EE">#EE82EE</span> | RGB(238, 130, 238) |
| VioletRed            | <span style="background-color:#D02090">#D02090</span> |  RGB(208, 32, 144) |
| Wheat                | <span style="background-color:#F5DEB3">#F5DEB3</span> | RGB(245, 222, 179) |
| White                | <span style="background-color:#FFFFFF">#FFFFFF</span> | RGB(255, 255, 255) |
| WhiteSmoke           | <span style="background-color:#F5F5F5">#F5F5F5</span> | RGB(245, 245, 245) |
| Yellow               | <span style="background-color:#FFFF00">#FFFF00</span> |   RGB(255, 255, 0) |
| YellowGreen          | <span style="background-color:#9ACD32">#9ACD32</span> |  RGB(154, 205, 50) |
