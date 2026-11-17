# Brutal Skyboxes source assets

Imported from the user's paid local Brutal Skyboxes v1.4.1 package:
`N:\rawmetal\src\assets\Skyboxes\Brutal`.

Donor masters stay as original lossless 4096 x 1024 PNGs in this folder.
The engine embeds neural `assets/ai/<stem>-x2.ntx` packets that reconstruct
through `texture-x2.ntm` (resource 348), the same AI path as the legacy
wasteland sky (resource 252). No donor PNG is modified.
The legacy skybox_plain_emx_14 asset and resource 252 remain separate.
No license text was present in the supplied extracted skybox directory; purchase
and distribution rights remain governed by the original asset package terms.

Original PNG payload: 185,144,525 bytes (176.57 MiB).
Embedded neural payload: 34,257,538 bytes (32.67 MiB).
Resource IDs 2800 through 2846 are stable and follow natural filename order.

Rebuild registration with `python tools/library/register_skyboxes.py`.
Verify it without changes with `python tools/library/register_skyboxes.py --check`.

| Resource | Original filename | Neural packet | PNG bytes | NTX bytes | PNG SHA-256 |
| --- | --- | --- | ---: | ---: | --- |
| 2800 | skybox_emx_1.png | skybox_emx_1-x2.ntx | 3771478 | 734562 | e6f97405383d9f68cc6b45868609d190413792a4b9fd7fb0f32e363e8a2911e7 |
| 2801 | skybox_emx_2.png | skybox_emx_2-x2.ntx | 5686702 | 1002732 | 6f789711e27d7e6fd5c91e026df950662873b133e17e9bb5290b90e47c4cba21 |
| 2802 | skybox_emx_3.png | skybox_emx_3-x2.ntx | 3356716 | 721622 | b3984755bdb2d30c770fb8be7f57f6cd55a2a5d9319330669fb59ddfa8bbf3d2 |
| 2803 | skybox_emx_4.png | skybox_emx_4-x2.ntx | 3975323 | 818994 | a7a5006bc97e093227010bd44ff4414892ca3cc6dd5dd4bf7c0b83d04e04e3eb |
| 2804 | skybox_emx_5.png | skybox_emx_5-x2.ntx | 3812704 | 656324 | 4da05daa2c1a1b23d3f1f9c0eb1ce61f42c0ff7f08751064a8532e5f82186ede |
| 2805 | skybox_emx_6.png | skybox_emx_6-x2.ntx | 6115205 | 1174070 | b4322fb1475ca9b249effbab980b1e50ac4d3e63133be0fc35bfc484cc635348 |
| 2806 | skybox_emx_8.png | skybox_emx_8-x2.ntx | 2650694 | 479332 | 67b4c07e7a52a86cdec9eb254af1d11158f9c8f5e1e581b1c31e9f6e8ea85e80 |
| 2807 | skybox_emx_9.png | skybox_emx_9-x2.ntx | 4117078 | 1033730 | 850d57cd2c7549656561b03666eedaa4158867bfdcc1dae55acf4767f016982c |
| 2808 | skybox_emx_10.png | skybox_emx_10-x2.ntx | 2640113 | 490246 | a783e4208edc84bff5f974054145564097dd8b67e4520d3b4686cd5de9b732ca |
| 2809 | skybox_emx_11.png | skybox_emx_11-x2.ntx | 3103757 | 618066 | b441a42f6bf6b9b4d1eb6b064ff8e8b2a278a22bcde18c81d6e1c272904d1c13 |
| 2810 | skybox_emx_12.png | skybox_emx_12-x2.ntx | 3655639 | 579506 | eaec65d247a688201e3ee63dee825151164a84dce18059a673784ef4e7afcb12 |
| 2811 | skybox_emx_13.png | skybox_emx_13-x2.ntx | 4491661 | 767704 | 2b91a4cf8f1db49122ad871b27ba554219a156106397c742fbb579675884268b |
| 2812 | skybox_emx_14.png | skybox_emx_14-x2.ntx | 5300246 | 1031288 | 87e3e062ac6e591f765931f7fdfefad61e011de283b08c7d0c8010e3e6faeb36 |
| 2813 | skybox_emx_15.png | skybox_emx_15-x2.ntx | 5660892 | 1121920 | 0a7f2b0c094faf45cc23997334953d979248f0d1dc92c225b800e75b47d4a12b |
| 2814 | skybox_emx_16.png | skybox_emx_16-x2.ntx | 4142772 | 804354 | 80c25f99a0b1a231fbf21a1f2403249f12b3133f73540c16968a2c399078bd69 |
| 2815 | skybox_emx_17.png | skybox_emx_17-x2.ntx | 4071305 | 815676 | 174ac3c8a3e28749f6ba0aefa95ee637d3f92a6f4fdd8a72ab65e1c6e120800d |
| 2816 | skybox_emx_18.png | skybox_emx_18-x2.ntx | 3919050 | 744598 | b739b64b27a1d1dade2eae48c8e204cc622dce697b90cd0921fae6914917e24b |
| 2817 | skybox_emx_19.png | skybox_emx_19-x2.ntx | 4095234 | 791550 | 707ae74cb29481af9cd155426d4d1153670033de07537ad534fc700efb2f3ecf |
| 2818 | skybox_emx_20.png | skybox_emx_20-x2.ntx | 6057787 | 1220034 | b9624c67ba5cb4ca8dc99656e0197b0523fefbd614ebbecb61caaf69622c622e |
| 2819 | skybox_emx_21.png | skybox_emx_21-x2.ntx | 4506728 | 988646 | f1db99e3fac61bf53d1a40e38dbb4f40f3835099f9defed4174e2256947152ed |
| 2820 | skybox_emx_23.png | skybox_emx_23-x2.ntx | 5278611 | 966200 | 041a6d99182d93b3fcb544de6b844568a62839fe41465e29bebb5059c1fb1321 |
| 2821 | skybox_emx_24.png | skybox_emx_24-x2.ntx | 5300752 | 1014546 | c2292b90c390dfa4212fe52ef0fdf9188b27648548e0dbb0298dc507902e9003 |
| 2822 | skybox_emx_25.png | skybox_emx_25-x2.ntx | 4948856 | 891366 | e14d02d08eb4b5af3826e0b5c237a25697b557f0125d8f22326ab5d6b2b0ef5e |
| 2823 | skybox_emx_26.png | skybox_emx_26-x2.ntx | 5378244 | 862216 | 606458fa4df4e72756cacb7aa0832dbeb16077bb94f7420dce036e79131768b4 |
| 2824 | skybox_emx_27.png | skybox_emx_27-x2.ntx | 5465152 | 958600 | 145f586aad2bda9ffb0dcb1f88b71b808517d9844b5871829f9482cf7499b67a |
| 2825 | skybox_emx_28.png | skybox_emx_28-x2.ntx | 6709157 | 1033910 | ac78a68b75294159f4d4f97dfce2724a58dc036d70b1f9fc00b0d39383241c05 |
| 2826 | skybox_emx_29.png | skybox_emx_29-x2.ntx | 7127284 | 1193540 | 251639a693d150d2c0209b55cc8d6b4eb6d5c4e50ff8a2ba8d297422e1e67ef5 |
| 2827 | skybox_emx_30.png | skybox_emx_30-x2.ntx | 7113945 | 1201818 | ba0e584d767c7769eef0aa619256f8fc483b04e9832e5847c88cf8d11f50d504 |
| 2828 | skybox_emx_31.png | skybox_emx_31-x2.ntx | 4874351 | 852324 | 217a667f37749e946ad597725fbb989383af347d50f7ac611ec6118da7b63d4c |
| 2829 | skybox_emx_32.png | skybox_emx_32-x2.ntx | 4783843 | 841380 | f97b9665b87bb3141ce48d36679c2fbb8923ada7abd76c94ff9c41808caf137a |
| 2830 | skybox_emx_33.png | skybox_emx_33-x2.ntx | 4785891 | 823642 | 7f3fa782d534cadb7357606c9ed69eae680db7d78ea2f6f9fc36b1075bbbec6d |
| 2831 | skybox_emx_34.png | skybox_emx_34-x2.ntx | 3546055 | 628210 | dc97ac502b4931d0a7cd9119d2e3f8cf7932bd109d696dbb7b7317dfe9ae8020 |
| 2832 | skybox_emx_35.png | skybox_emx_35-x2.ntx | 3442857 | 612786 | bde19cccae358d1df543c0b3597521b69a6e200a785e8dc1e532a20d893785a8 |
| 2833 | skybox_plain_emx_1.png | skybox_plain_emx_1-x2.ntx | 2001711 | 330108 | f9c2fd888f4283fa87cadd72823fdd445533888d72c75811b50aff03ef391528 |
| 2834 | skybox_plain_emx_2.png | skybox_plain_emx_2-x2.ntx | 1631181 | 229718 | d6f016530e06df887541f0071b31e4475ca0e940b61ba321f38c822dece9121a |
| 2835 | skybox_plain_emx_3.png | skybox_plain_emx_3-x2.ntx | 1749921 | 269844 | ab2baa3815ebebaf694dd3c32a706d1199e2b40f21fe989bfeed229a15c34de8 |
| 2836 | skybox_plain_emx_4.png | skybox_plain_emx_4-x2.ntx | 2276676 | 392692 | 8db75185cad6697b2d9ded506647a7d4ac1a1a05632d6a100bf20de582122467 |
| 2837 | skybox_plain_emx_5.png | skybox_plain_emx_5-x2.ntx | 1961279 | 310272 | 9d7afb56c8663f6249c471bcbdbd49a5a66607ff58ab13a995a3230a741513f4 |
| 2838 | skybox_plain_emx_6.png | skybox_plain_emx_6-x2.ntx | 2523100 | 482614 | 6848ef423ccc52c91e298a917151a76ddb7770326d2effaf7d79a45c9e18101a |
| 2839 | skybox_plain_emx_7.png | skybox_plain_emx_7-x2.ntx | 3290309 | 706252 | d2c17f585cbeb6fb36145ff4a1f4aba2388fe67b8a906edb2f3e7317ac2a5a51 |
| 2840 | skybox_plain_emx_8.png | skybox_plain_emx_8-x2.ntx | 3833979 | 733496 | 792acf226945bd45dc16dc48f2cf4416fe5fa8cfc07b1ac307ec037c19e0237a |
| 2841 | skybox_plain_emx_9.png | skybox_plain_emx_9-x2.ntx | 2820571 | 491814 | 64497080ae06aa0206c9d17d5ee848186ce1a1ba4ae1ccce387bf1ac0df51504 |
| 2842 | skybox_plain_emx_10.png | skybox_plain_emx_10-x2.ntx | 1604216 | 319926 | 1cec2324285e85c051b6d40e3163541efd3c60a61429a15d391b0b39e74b6485 |
| 2843 | skybox_plain_emx_11.png | skybox_plain_emx_11-x2.ntx | 1460048 | 277962 | ed6ea7ca7b6d8491615587c4ce048dbfabbed55fa80fd23da62ee9b172d61113 |
| 2844 | skybox_plain_emx_12.png | skybox_plain_emx_12-x2.ntx | 3107756 | 599802 | 32807a66f0fb29ef9f3de0f2b391c73b7c108c8ddaf8cb53f09ef1da53bb5e88 |
| 2845 | skybox_plain_emx_13.png | skybox_plain_emx_13-x2.ntx | 1563447 | 350884 | c4da4faf61bb1d1ba3b5f4bcbe429d37492f4f7deee3e8819b1a6067e71eea51 |
| 2846 | skybox_plain_emx_14.png | skybox_plain_emx_14-x2.ntx | 1434249 | 286662 | b1726c802f6a70c25c99985b920a93a4cca18f3c62d96907e9e05ff71a99f7e4 |
