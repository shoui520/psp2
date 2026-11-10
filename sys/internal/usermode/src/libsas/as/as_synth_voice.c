/*
 * The SYNTH rack's task: each voice's source played through the resampler at its pitch,
 * shaped by its envelope, filters and distortion, and mixed into its sends' busses.
 */

#include <string.h>

#include "as_ssp.h"
#include "as_math.h"
#include "as_synth.h"
#include "as_util.h"

typedef SceInt16 SceSasAsSynthTaps[4];

/* the resampler's 8-tap filter, in 512 phases and the end: taps 0..3 from phase p, taps 7..4 from phase 512 - p (2.14) */
static const SceSasAsSynthTaps sceSasAsSynthResampleTable[513] = {
	{ 0, 0, 0, 16384 }, { -1, 7, -27, 16383 }, { -3, 15, -54, 16383 }, { -4, 23, -81, 16382 },
	{ -6, 31, -108, 16382 }, { -7, 39, -134, 16381 }, { -9, 47, -161, 16379 },
	{ -10, 55, -187, 16378 }, { -12, 62, -213, 16376 }, { -13, 70, -240, 16374 },
	{ -14, 77, -266, 16372 }, { -16, 85, -291, 16370 }, { -17, 93, -317, 16367 },
	{ -19, 100, -343, 16365 }, { -20, 107, -368, 16361 }, { -21, 115, -393, 16358 },
	{ -23, 122, -418, 16355 }, { -24, 130, -443, 16351 }, { -25, 137, -468, 16347 },
	{ -27, 144, -493, 16343 }, { -28, 151, -517, 16339 }, { -29, 158, -542, 16334 },
	{ -31, 165, -566, 16329 }, { -32, 173, -590, 16324 }, { -33, 180, -614, 16319 },
	{ -34, 187, -638, 16313 }, { -36, 193, -661, 16308 }, { -37, 200, -685, 16302 },
	{ -38, 207, -708, 16295 }, { -39, 214, -731, 16289 }, { -40, 221, -754, 16283 },
	{ -41, 227, -777, 16276 }, { -43, 234, -800, 16269 }, { -44, 241, -822, 16261 },
	{ -45, 247, -845, 16254 }, { -46, 254, -867, 16246 }, { -47, 260, -889, 16238 },
	{ -48, 267, -911, 16230 }, { -49, 273, -933, 16222 }, { -50, 279, -955, 16213 },
	{ -51, 286, -976, 16204 }, { -52, 292, -998, 16195 }, { -53, 298, -1019, 16186 },
	{ -54, 304, -1040, 16176 }, { -55, 310, -1061, 16167 }, { -56, 316, -1082, 16157 },
	{ -57, 323, -1103, 16147 }, { -58, 328, -1123, 16136 }, { -59, 334, -1144, 16126 },
	{ -60, 340, -1164, 16115 }, { -61, 346, -1184, 16104 }, { -62, 352, -1204, 16093 },
	{ -62, 358, -1224, 16081 }, { -63, 363, -1243, 16070 }, { -64, 369, -1263, 16058 },
	{ -65, 375, -1282, 16046 }, { -66, 380, -1301, 16033 }, { -67, 386, -1320, 16021 },
	{ -67, 391, -1339, 16008 }, { -68, 397, -1358, 15995 }, { -69, 402, -1376, 15982 },
	{ -70, 407, -1395, 15969 }, { -71, 413, -1413, 15955 }, { -71, 418, -1431, 15941 },
	{ -72, 423, -1449, 15927 }, { -73, 428, -1467, 15913 }, { -73, 433, -1485, 15899 },
	{ -74, 438, -1502, 15884 }, { -75, 443, -1520, 15869 }, { -75, 448, -1537, 15854 },
	{ -76, 453, -1554, 15839 }, { -77, 458, -1571, 15823 }, { -77, 463, -1588, 15807 },
	{ -78, 468, -1604, 15791 }, { -79, 472, -1621, 15775 }, { -79, 477, -1637, 15759 },
	{ -80, 482, -1653, 15742 }, { -80, 486, -1669, 15726 }, { -81, 491, -1685, 15709 },
	{ -81, 495, -1701, 15692 }, { -82, 500, -1717, 15674 }, { -82, 504, -1732, 15657 },
	{ -83, 509, -1747, 15639 }, { -83, 513, -1763, 15621 }, { -84, 517, -1778, 15603 },
	{ -84, 521, -1792, 15584 }, { -85, 525, -1807, 15566 }, { -85, 530, -1822, 15547 },
	{ -86, 534, -1836, 15528 }, { -86, 538, -1850, 15509 }, { -87, 542, -1865, 15489 },
	{ -87, 546, -1879, 15470 }, { -87, 550, -1892, 15450 }, { -88, 553, -1906, 15430 },
	{ -88, 557, -1920, 15410 }, { -89, 561, -1933, 15389 }, { -89, 565, -1946, 15369 },
	{ -89, 568, -1959, 15348 }, { -90, 572, -1972, 15327 }, { -90, 575, -1985, 15306 },
	{ -90, 579, -1998, 15284 }, { -91, 582, -2010, 15263 }, { -91, 586, -2023, 15241 },
	{ -91, 589, -2035, 15219 }, { -91, 593, -2047, 15197 }, { -92, 596, -2059, 15174 },
	{ -92, 599, -2071, 15152 }, { -92, 602, -2082, 15129 }, { -92, 605, -2094, 15106 },
	{ -93, 609, -2105, 15083 }, { -93, 612, -2116, 15060 }, { -93, 615, -2127, 15036 },
	{ -93, 618, -2138, 15013 }, { -93, 621, -2149, 14989 }, { -94, 623, -2160, 14965 },
	{ -94, 626, -2170, 14940 }, { -94, 629, -2181, 14916 }, { -94, 632, -2191, 14891 },
	{ -94, 635, -2201, 14867 }, { -94, 637, -2211, 14842 }, { -94, 640, -2221, 14816 },
	{ -95, 642, -2230, 14791 }, { -95, 645, -2240, 14766 }, { -95, 647, -2249, 14740 },
	{ -95, 650, -2259, 14714 }, { -95, 652, -2268, 14688 }, { -95, 655, -2277, 14662 },
	{ -95, 657, -2285, 14635 }, { -95, 659, -2294, 14609 }, { -95, 661, -2303, 14582 },
	{ -95, 664, -2311, 14555 }, { -95, 666, -2319, 14528 }, { -95, 668, -2327, 14500 },
	{ -95, 670, -2335, 14473 }, { -95, 672, -2343, 14445 }, { -95, 674, -2351, 14417 },
	{ -95, 676, -2359, 14389 }, { -95, 678, -2366, 14361 }, { -95, 679, -2373, 14333 },
	{ -95, 681, -2381, 14304 }, { -95, 683, -2388, 14276 }, { -95, 685, -2394, 14247 },
	{ -95, 686, -2401, 14218 }, { -95, 688, -2408, 14189 }, { -95, 689, -2414, 14159 },
	{ -95, 691, -2421, 14130 }, { -95, 693, -2427, 14100 }, { -95, 694, -2433, 14070 },
	{ -94, 695, -2439, 14040 }, { -94, 697, -2445, 14010 }, { -94, 698, -2451, 13980 },
	{ -94, 699, -2456, 13949 }, { -94, 701, -2462, 13919 }, { -94, 702, -2467, 13888 },
	{ -94, 703, -2472, 13857 }, { -93, 704, -2477, 13826 }, { -93, 705, -2482, 13794 },
	{ -93, 706, -2487, 13763 }, { -93, 707, -2492, 13731 }, { -93, 708, -2496, 13700 },
	{ -93, 709, -2501, 13668 }, { -92, 710, -2505, 13636 }, { -92, 711, -2509, 13603 },
	{ -92, 712, -2513, 13571 }, { -92, 713, -2517, 13539 }, { -92, 713, -2521, 13506 },
	{ -91, 714, -2525, 13473 }, { -91, 715, -2528, 13440 }, { -91, 715, -2532, 13407 },
	{ -91, 716, -2535, 13374 }, { -90, 716, -2538, 13340 }, { -90, 717, -2541, 13307 },
	{ -90, 717, -2544, 13273 }, { -90, 718, -2547, 13239 }, { -89, 718, -2549, 13206 },
	{ -89, 719, -2552, 13171 }, { -89, 719, -2554, 13137 }, { -89, 719, -2557, 13103 },
	{ -88, 720, -2559, 13068 }, { -88, 720, -2561, 13034 }, { -88, 720, -2563, 12999 },
	{ -87, 720, -2565, 12964 }, { -87, 720, -2567, 12929 }, { -87, 720, -2568, 12894 },
	{ -86, 720, -2570, 12858 }, { -86, 720, -2571, 12823 }, { -86, 720, -2572, 12787 },
	{ -85, 720, -2573, 12752 }, { -85, 720, -2575, 12716 }, { -85, 720, -2575, 12680 },
	{ -84, 720, -2576, 12644 }, { -84, 720, -2577, 12608 }, { -84, 719, -2577, 12571 },
	{ -83, 719, -2578, 12535 }, { -83, 719, -2578, 12498 }, { -83, 719, -2579, 12462 },
	{ -82, 718, -2579, 12425 }, { -82, 718, -2579, 12388 }, { -82, 717, -2579, 12351 },
	{ -81, 717, -2578, 12314 }, { -81, 716, -2578, 12276 }, { -80, 716, -2578, 12239 },
	{ -80, 715, -2577, 12202 }, { -80, 715, -2577, 12164 }, { -79, 714, -2576, 12126 },
	{ -79, 713, -2575, 12088 }, { -79, 713, -2574, 12050 }, { -78, 712, -2573, 12012 },
	{ -78, 711, -2572, 11974 }, { -77, 710, -2570, 11936 }, { -77, 710, -2569, 11897 },
	{ -76, 709, -2568, 11859 }, { -76, 708, -2566, 11820 }, { -76, 707, -2564, 11782 },
	{ -75, 706, -2563, 11743 }, { -75, 705, -2561, 11704 }, { -74, 704, -2559, 11665 },
	{ -74, 703, -2557, 11626 }, { -74, 702, -2554, 11586 }, { -73, 701, -2552, 11547 },
	{ -73, 700, -2550, 11508 }, { -72, 699, -2547, 11468 }, { -72, 698, -2545, 11429 },
	{ -71, 696, -2542, 11389 }, { -71, 695, -2539, 11349 }, { -70, 694, -2536, 11309 },
	{ -70, 693, -2533, 11269 }, { -70, 691, -2530, 11229 }, { -69, 690, -2527, 11189 },
	{ -69, 689, -2524, 11149 }, { -68, 687, -2521, 11109 }, { -68, 686, -2517, 11068 },
	{ -67, 685, -2514, 11028 }, { -67, 683, -2510, 10987 }, { -66, 682, -2506, 10946 },
	{ -66, 680, -2503, 10906 }, { -66, 679, -2499, 10865 }, { -65, 677, -2495, 10824 },
	{ -65, 676, -2491, 10783 }, { -64, 674, -2487, 10742 }, { -64, 672, -2482, 10701 },
	{ -63, 671, -2478, 10659 }, { -63, 669, -2474, 10618 }, { -62, 667, -2469, 10577 },
	{ -62, 666, -2465, 10535 }, { -61, 664, -2460, 10494 }, { -61, 662, -2455, 10452 },
	{ -60, 660, -2450, 10411 }, { -60, 659, -2445, 10369 }, { -59, 657, -2440, 10327 },
	{ -59, 655, -2435, 10285 }, { -59, 653, -2430, 10243 }, { -58, 651, -2425, 10201 },
	{ -58, 649, -2420, 10159 }, { -57, 647, -2414, 10117 }, { -57, 645, -2409, 10075 },
	{ -56, 643, -2403, 10033 }, { -56, 641, -2398, 9991 }, { -55, 639, -2392, 9948 },
	{ -55, 637, -2386, 9906 }, { -54, 635, -2380, 9863 }, { -54, 633, -2374, 9821 },
	{ -53, 631, -2368, 9778 }, { -53, 629, -2362, 9736 }, { -52, 627, -2356, 9693 },
	{ -52, 625, -2350, 9650 }, { -52, 623, -2343, 9608 }, { -51, 620, -2337, 9565 },
	{ -51, 618, -2331, 9522 }, { -50, 616, -2324, 9479 }, { -50, 614, -2318, 9436 },
	{ -49, 612, -2311, 9393 }, { -49, 609, -2304, 9350 }, { -48, 607, -2297, 9307 },
	{ -48, 605, -2290, 9264 }, { -47, 602, -2284, 9221 }, { -47, 600, -2277, 9177 },
	{ -46, 598, -2270, 9134 }, { -46, 595, -2262, 9091 }, { -46, 593, -2255, 9048 },
	{ -45, 591, -2248, 9004 }, { -45, 588, -2241, 8961 }, { -44, 586, -2233, 8917 },
	{ -44, 583, -2226, 8874 }, { -43, 581, -2218, 8830 }, { -43, 578, -2211, 8787 },
	{ -42, 576, -2203, 8743 }, { -42, 573, -2195, 8700 }, { -41, 571, -2188, 8656 },
	{ -41, 568, -2180, 8613 }, { -41, 566, -2172, 8569 }, { -40, 563, -2164, 8525 },
	{ -40, 561, -2156, 8482 }, { -39, 558, -2148, 8438 }, { -39, 555, -2140, 8394 },
	{ -38, 553, -2132, 8350 }, { -38, 550, -2124, 8307 }, { -38, 548, -2115, 8263 },
	{ -37, 545, -2107, 8219 }, { -37, 542, -2099, 8175 }, { -36, 540, -2090, 8131 },
	{ -36, 537, -2082, 8088 }, { -35, 534, -2073, 8044 }, { -35, 532, -2065, 8000 },
	{ -35, 529, -2056, 7956 }, { -34, 526, -2048, 7912 }, { -34, 524, -2039, 7868 },
	{ -33, 521, -2030, 7824 }, { -33, 518, -2021, 7780 }, { -32, 515, -2013, 7737 },
	{ -32, 513, -2004, 7693 }, { -32, 510, -1995, 7649 }, { -31, 507, -1986, 7605 },
	{ -31, 504, -1977, 7561 }, { -30, 502, -1968, 7517 }, { -30, 499, -1959, 7473 },
	{ -30, 496, -1949, 7429 }, { -29, 493, -1940, 7385 }, { -29, 490, -1931, 7341 },
	{ -29, 487, -1922, 7297 }, { -28, 485, -1912, 7253 }, { -28, 482, -1903, 7210 },
	{ -27, 479, -1894, 7166 }, { -27, 476, -1884, 7122 }, { -27, 473, -1875, 7078 },
	{ -26, 470, -1865, 7034 }, { -26, 468, -1856, 6990 }, { -25, 465, -1846, 6946 },
	{ -25, 462, -1836, 6903 }, { -25, 459, -1827, 6859 }, { -24, 456, -1817, 6815 },
	{ -24, 453, -1807, 6771 }, { -24, 450, -1798, 6728 }, { -23, 447, -1788, 6684 },
	{ -23, 444, -1778, 6640 }, { -23, 441, -1768, 6596 }, { -22, 439, -1758, 6553 },
	{ -22, 436, -1748, 6509 }, { -22, 433, -1738, 6466 }, { -21, 430, -1729, 6422 },
	{ -21, 427, -1719, 6378 }, { -21, 424, -1708, 6335 }, { -20, 421, -1698, 6291 },
	{ -20, 418, -1688, 6248 }, { -20, 415, -1678, 6204 }, { -19, 412, -1668, 6161 },
	{ -19, 409, -1658, 6118 }, { -19, 406, -1648, 6074 }, { -18, 403, -1638, 6031 },
	{ -18, 400, -1627, 5988 }, { -18, 397, -1617, 5945 }, { -17, 394, -1607, 5901 },
	{ -17, 391, -1597, 5858 }, { -17, 389, -1586, 5815 }, { -17, 386, -1576, 5772 },
	{ -16, 383, -1565, 5729 }, { -16, 380, -1555, 5686 }, { -16, 377, -1545, 5643 },
	{ -15, 374, -1534, 5600 }, { -15, 371, -1524, 5557 }, { -15, 368, -1513, 5514 },
	{ -15, 365, -1503, 5471 }, { -14, 362, -1492, 5429 }, { -14, 359, -1482, 5386 },
	{ -14, 356, -1471, 5343 }, { -13, 353, -1461, 5301 }, { -13, 350, -1450, 5258 },
	{ -13, 347, -1440, 5216 }, { -13, 344, -1429, 5173 }, { -12, 341, -1419, 5131 },
	{ -12, 338, -1408, 5089 }, { -12, 335, -1397, 5046 }, { -12, 332, -1387, 5004 },
	{ -11, 330, -1376, 4962 }, { -11, 327, -1366, 4920 }, { -11, 324, -1355, 4878 },
	{ -11, 321, -1344, 4836 }, { -10, 318, -1334, 4794 }, { -10, 315, -1323, 4752 },
	{ -10, 312, -1312, 4710 }, { -10, 309, -1302, 4668 }, { -10, 306, -1291, 4627 },
	{ -9, 303, -1280, 4585 }, { -9, 300, -1269, 4544 }, { -9, 297, -1259, 4502 },
	{ -9, 295, -1248, 4461 }, { -9, 292, -1237, 4419 }, { -8, 289, -1227, 4378 },
	{ -8, 286, -1216, 4337 }, { -8, 283, -1205, 4296 }, { -8, 280, -1194, 4255 },
	{ -8, 277, -1184, 4214 }, { -7, 274, -1173, 4173 }, { -7, 272, -1162, 4132 },
	{ -7, 269, -1151, 4091 }, { -7, 266, -1141, 4050 }, { -7, 263, -1130, 4010 },
	{ -6, 260, -1119, 3969 }, { -6, 257, -1108, 3929 }, { -6, 255, -1098, 3888 },
	{ -6, 252, -1087, 3848 }, { -6, 249, -1076, 3808 }, { -6, 246, -1066, 3768 },
	{ -5, 243, -1055, 3728 }, { -5, 241, -1044, 3688 }, { -5, 238, -1033, 3648 },
	{ -5, 235, -1023, 3608 }, { -5, 232, -1012, 3568 }, { -5, 230, -1001, 3529 },
	{ -5, 227, -991, 3489 }, { -4, 224, -980, 3450 }, { -4, 221, -969, 3410 },
	{ -4, 219, -959, 3371 }, { -4, 216, -948, 3332 }, { -4, 213, -937, 3293 },
	{ -4, 210, -927, 3254 }, { -4, 208, -916, 3215 }, { -3, 205, -905, 3176 },
	{ -3, 202, -895, 3137 }, { -3, 200, -884, 3098 }, { -3, 197, -874, 3060 },
	{ -3, 194, -863, 3021 }, { -3, 192, -853, 2983 }, { -3, 189, -842, 2945 },
	{ -3, 186, -831, 2907 }, { -3, 184, -821, 2869 }, { -2, 181, -810, 2831 },
	{ -2, 179, -800, 2793 }, { -2, 176, -790, 2755 }, { -2, 173, -779, 2717 },
	{ -2, 171, -769, 2680 }, { -2, 168, -758, 2642 }, { -2, 166, -748, 2605 },
	{ -2, 163, -737, 2568 }, { -2, 161, -727, 2530 }, { -2, 158, -717, 2493 },
	{ -2, 156, -706, 2456 }, { -1, 153, -696, 2420 }, { -1, 151, -686, 2383 },
	{ -1, 148, -675, 2346 }, { -1, 146, -665, 2310 }, { -1, 143, -655, 2273 },
	{ -1, 141, -645, 2237 }, { -1, 138, -635, 2201 }, { -1, 136, -624, 2165 },
	{ -1, 134, -614, 2129 }, { -1, 131, -604, 2093 }, { -1, 129, -594, 2057 },
	{ -1, 126, -584, 2022 }, { -1, 124, -574, 1986 }, { -1, 122, -564, 1951 },
	{ -1, 119, -554, 1915 }, { -1, 117, -544, 1880 }, { 0, 115, -534, 1845 },
	{ 0, 112, -524, 1810 }, { 0, 110, -514, 1775 }, { 0, 108, -504, 1741 },
	{ 0, 105, -494, 1706 }, { 0, 103, -484, 1672 }, { 0, 101, -474, 1637 },
	{ 0, 99, -465, 1603 }, { 0, 96, -455, 1569 }, { 0, 94, -445, 1535 }, { 0, 92, -435, 1501 },
	{ 0, 90, -426, 1467 }, { 0, 88, -416, 1434 }, { 0, 85, -407, 1400 }, { 0, 83, -397, 1367 },
	{ 0, 81, -387, 1333 }, { 0, 79, -378, 1300 }, { 0, 77, -368, 1267 }, { 0, 75, -359, 1234 },
	{ 0, 73, -349, 1202 }, { 0, 71, -340, 1169 }, { 0, 69, -331, 1137 }, { 0, 66, -321, 1104 },
	{ 0, 64, -312, 1072 }, { 0, 62, -303, 1040 }, { 0, 60, -293, 1008 }, { 0, 58, -284, 976 },
	{ 0, 56, -275, 944 }, { 0, 54, -266, 913 }, { 0, 52, -257, 881 }, { 0, 50, -248, 850 },
	{ 0, 49, -239, 819 }, { 0, 47, -229, 787 }, { 0, 45, -220, 757 }, { 0, 43, -212, 726 },
	{ 0, 41, -203, 695 }, { 0, 39, -194, 664 }, { 0, 37, -185, 634 }, { 0, 35, -176, 604 },
	{ 0, 33, -167, 574 }, { 0, 32, -159, 544 }, { 0, 30, -150, 514 }, { 0, 28, -141, 484 },
	{ 0, 26, -133, 454 }, { 0, 24, -124, 425 }, { 0, 23, -115, 396 }, { 0, 21, -107, 366 },
	{ 0, 19, -98, 337 }, { 0, 18, -90, 308 }, { 0, 16, -81, 280 }, { 0, 14, -73, 251 },
	{ 0, 12, -65, 223 }, { 0, 11, -56, 194 }, { 0, 9, -48, 166 }, { 0, 7, -40, 138 },
	{ 0, 6, -32, 110 }, { 0, 4, -24, 82 }, { 0, 3, -16, 54 }, { 0, 1, -8, 27 }, { 0, 0, 0, 0 },
};

/* the ADPCM predictors, four taps (3.13) */
static const SceSasAsSynthTaps sceSasAsSynthAdpcmTable[128] = {
	{ 0, 0, 0, 0 }, { 7680, 0, 0, 0 }, { 14720, -6656, 0, 0 }, { 12544, -7040, 0, 0 },
	{ 15616, -7680, 0, 0 }, { 14731, -7059, 0, 0 }, { 14507, -7366, 0, 0 },
	{ 13920, -7522, 0, 0 }, { 13133, -7680, 0, 0 }, { 12028, -7680, 0, 0 },
	{ 10764, -7680, 0, 0 }, { 9359, -7680, 0, 0 }, { 7832, -7680, 0, 0 },
	{ 6201, -7680, 0, 0 }, { 4488, -7680, 0, 0 }, { 2717, -7680, 0, 0 }, { 910, -7680, 0, 0 },
	{ -910, -7680, 0, 0 }, { -2717, -7680, 0, 0 }, { -4488, -7680, 0, 0 },
	{ -6201, -7680, 0, 0 }, { -7832, -7680, 0, 0 }, { -9359, -7680, 0, 0 },
	{ -10764, -7680, 0, 0 }, { -12028, -7680, 0, 0 }, { -13133, -7680, 0, 0 },
	{ -13920, -7522, 0, 0 }, { -14507, -7366, 0, 0 }, { -14731, -7059, 0, 0 },
	{ 5376, -9216, 3328, -3072 }, { -6400, -7168, -3328, -2304 },
	{ -10496, -7424, -3584, -1024 }, { -167, -2722, -494, -541 }, { -7430, -2221, -2298, 424 },
	{ -8001, -3166, -2814, 289 }, { 6018, -4750, 2649, -1298 }, { 3798, -6946, 3875, -1216 },
	{ -8237, -2596, -2071, 227 }, { 9199, 1982, -1382, -2316 }, { 13021, -3044, -3792, 1267 },
	{ 13112, -4487, -2250, 1665 }, { -1668, -3744, -6456, 840 }, { 7819, -4328, 2111, -506 },
	{ 9571, -1336, -757, 487 }, { 10032, -2562, 300, 199 }, { -4745, -4122, -5486, -1493 },
	{ -5896, 2378, -4787, -6947 }, { -1193, -9117, -1237, -3114 },
	{ 2783, -7108, -1575, -1447 }, { -7334, -2062, -2212, 446 }, { 6127, -2577, -315, -18 },
	{ 9457, -1858, 102, 258 }, { 7876, -4483, 2126, -538 }, { -7172, -1795, -2069, 482 },
	{ -7358, -2102, -2233, 440 }, { -9170, -3509, -2674, -391 },
	{ -2638, -2647, -1929, -1637 }, { 1873, 9183, 1860, -5746 }, { 9214, 1859, -1124, -2427 },
	{ 13204, -3012, -4139, 1370 }, { 12437, -4792, -256, 622 }, { -2653, -1144, -3182, -6878 },
	{ 9331, -1048, -828, 507 }, { 1642, -620, -946, -4229 }, { 4246, -7585, -533, -2259 },
	{ -8988, -3891, -2807, 44 }, { -2562, -2735, -1730, -1899 }, { 3182, -483, -714, -1421 },
	{ 7937, -3844, 2821, -1019 }, { 10069, -2609, 314, 195 }, { 8400, -3297, 1551, -155 },
	{ -8529, -2775, -2432, -336 }, { 9477, -1882, 108, 256 }, { 75, -2241, -298, -6937 },
	{ -9143, -4160, -2963, 5 }, { -7270, -1958, -2156, 460 }, { -2740, 3745, 5936, -1089 },
	{ 8993, 1948, -683, -2704 }, { 13101, -2835, -3854, 1055 }, { 9543, -1961, 130, 250 },
	{ 5272, -4270, 3124, -3157 }, { -7696, -3383, -2907, -456 }, { 7309, 2523, 434, -2461 },
	{ 10275, -2867, 391, 172 }, { 10940, -3721, 665, 97 }, { 24, -310, -1262, 320 },
	{ -8122, -2411, -2311, -271 }, { -8511, -3067, -2337, 163 }, { 326, -3846, 419, -933 },
	{ 8895, 2194, -541, -2880 }, { 12073, -1876, -2017, -601 }, { 8729, -3423, 1674, -169 },
	{ 12950, -3847, -3007, 1946 }, { 10038, -2570, 302, 198 }, { 9385, -2757, 1008, 41 },
	{ -4720, -5006, -2852, -1161 }, { 7869, -4326, 2135, -501 }, { 2450, -8597, 1299, -2780 },
	{ 10192, -2763, 360, 181 }, { 11313, -4213, 833, 53 }, { 10154, -2716, 345, 185 },
	{ 9638, -1417, -737, 482 }, { 3854, -4554, 2843, -3397 }, { 6699, -5659, 2249, -1074 },
	{ 11082, -3908, 728, 80 }, { -1026, -9810, -805, -3462 }, { 10396, -3746, 1367, -96 },
	{ 10287, 988, -1915, -1437 }, { 7953, 3878, -764, -3263 }, { 12689, -3375, -3354, 2079 },
	{ 6641, 3166, 231, -2089 }, { -2348, -7354, -1944, -4122 }, { 9290, -4039, 1885, -246 },
	{ 4633, -6403, 1748, -1619 }, { 11247, -4125, 802, 61 }, { 9807, -2284, 219, 222 },
	{ 9736, -1536, -706, 473 }, { 8440, -3436, 1562, -176 }, { 9307, -1021, -835, 509 },
	{ 1698, -9025, 688, -3037 }, { 10214, -2791, 368, 179 }, { 8390, 3248, -758, -2989 },
	{ 7201, 3316, 46, -2614 }, { -88, -7809, -538, -4571 }, { 6193, -5189, 2760, -1245 },
	{ 12325, -1290, -3284, 253 }, { 13064, -4075, -2824, 1877 }, { 5333, 2999, 775, -1132 },
};

/* the short ADPCM predictors, two taps (3.13) */
static const SceSasAsSynthTaps sceSasAsSynthAdpcmShortTable[16] = {
	{ 0, 0, 0, 0 }, { 7680, 0, 0, 0 }, { 14720, -6656, 0, 0 }, { 12544, -7040, 0, 0 },
	{ 15616, -7680, 0, 0 }, { 3840, 0, 0, 0 }, { 7360, -3328, 0, 0 }, { 6272, -3520, 0, 0 },
	{ 7808, -3840, 0, 0 }, { 1920, 0, 0, 0 }, { 3680, -1664, 0, 0 }, { 3136, -1760, 0, 0 },
	{ 3904, -1920, 0, 0 }, { 4096, -7680, 0, 0 }, { 1920, -7680, 0, 0 }, { 896, -7680, 0, 0 },
};

/* a quarter of a sine wave, its end included (1.15) */
static const SceInt16 sceSasAsSynthSineTable[257] = {
	0, 201, 402, 603, 804, 1005, 1206, 1406, 1607, 1808, 2009, 2209, 2410, 2610, 2811, 3011,
	3211, 3411, 3611, 3811, 4011, 4210, 4409, 4608, 4807, 5006, 5205, 5403, 5601, 5799, 5997,
	6195, 6392, 6589, 6786, 6982, 7179, 7375, 7571, 7766, 7961, 8156, 8351, 8545, 8739, 8932,
	9126, 9319, 9511, 9703, 9895, 10087, 10278, 10469, 10659, 10849, 11038, 11227, 11416,
	11604, 11792, 11980, 12166, 12353, 12539, 12724, 12909, 13094, 13278, 13462, 13645, 13827,
	14009, 14191, 14372, 14552, 14732, 14911, 15090, 15268, 15446, 15623, 15799, 15975, 16150,
	16325, 16499, 16672, 16845, 17017, 17189, 17360, 17530, 17699, 17868, 18036, 18204, 18371,
	18537, 18702, 18867, 19031, 19194, 19357, 19519, 19680, 19840, 20000, 20159, 20317, 20474,
	20631, 20787, 20942, 21096, 21249, 21402, 21554, 21705, 21855, 22004, 22153, 22301, 22448,
	22594, 22739, 22883, 23027, 23169, 23311, 23452, 23592, 23731, 23869, 24006, 24143, 24278,
	24413, 24546, 24679, 24811, 24942, 25072, 25201, 25329, 25456, 25582, 25707, 25831, 25954,
	26077, 26198, 26318, 26437, 26556, 26673, 26789, 26905, 27019, 27132, 27244, 27355, 27466,
	27575, 27683, 27790, 27896, 28001, 28105, 28208, 28309, 28410, 28510, 28608, 28706, 28802,
	28897, 28992, 29085, 29177, 29268, 29358, 29446, 29534, 29621, 29706, 29790, 29873, 29955,
	30036, 30116, 30195, 30272, 30349, 30424, 30498, 30571, 30643, 30713, 30783, 30851, 30918,
	30984, 31049, 31113, 31175, 31236, 31297, 31356, 31413, 31470, 31525, 31580, 31633, 31684,
	31735, 31785, 31833, 31880, 31926, 31970, 32014, 32056, 32097, 32137, 32176, 32213, 32249,
	32284, 32318, 32350, 32382, 32412, 32441, 32468, 32495, 32520, 32544, 32567, 32588, 32609,
	32628, 32646, 32662, 32678, 32692, 32705, 32717, 32727, 32736, 32744, 32751, 32757, 32761,
	32764, 32766, 32767,
};

SceSize sceSasAsSynthVoiceIteratorGetSize(const SceSasAsRenderInfo *render)
{
	return sizeof(SceSasAsSynthVoiceIterator) + ((render->numBusses + 31u) >> 5) * sizeof(SceUInt32);
}

SceSasAsSynthVoiceIterator *sceSasAsSynthVoiceIteratorBegin(const SceSasAsRenderInfo *render,
							   SceSasAsSynthTaskInfo *info,
							   void *buffer)
{
	SceSasAsSynthVoiceIterator *iterator = buffer;

	iterator->info = info;
	iterator->render = render;
	iterator->index = -1;
	iterator->usedBusses = iterator->bitmap;
	sceSasAsMemZero(iterator->bitmap, ((render->numBusses + 31u) >> 5) * sizeof(SceUInt32));
	return iterator;
}

/* clears the busses no voice mixed into */
void sceSasAsSynthVoiceIteratorEnd(SceSasAsSynthVoiceIterator *iterator)
{
	SceUInt32 buss, channel;
	SceInt32 **buffers;

	for (buss = 0; buss < iterator->render->numBusses; buss++) {
		if ((iterator->usedBusses[buss >> 5] >> (buss & 31)) & 1)
			continue;
		buffers = sceSasAsSynthVoiceIteratorGetBuss(iterator, buss, NULL);
		for (channel = 0; channel < iterator->render->numChannels; channel++)
			sceSasAsSynthMixReplace(buffers[channel], NULL, iterator->render->granularity,
						0, 0);
	}
}

/* the next voice with work to do; the others report nothing */
int sceSasAsSynthVoiceIteratorNext(SceSasAsSynthVoiceIterator *iterator)
{
	SceSasAsSynthTaskInfo *info = iterator->info;
	SceSasAsSynthVoiceParam *param;
	SceSasAsSynthVoiceWork *work;
	SceSasAsSynthVoiceStatus *status;

	if (iterator->index >= info->numVoices)
		return 0;
	for (;;) {
		iterator->index++;
		if (iterator->index >= info->numVoices)
			return 0;
		param = sceSasAsSynthVoiceIteratorGetParam(iterator);
		work = sceSasAsSynthVoiceIteratorGetWork(iterator);
		if (work->keyOnCount != param->keyOnCount)
			return 1;
		if (work->flags != 0 && (param->events & SCE_SAS_AS_SYNTH_KEY_ON) &&
		    (!(param->events & SCE_SAS_AS_SYNTH_STOP) ||
		     (work->flags & SCE_SAS_AS_SYNTH_VOICE_ACTIVE)))
			return 1;
		status = sceSasAsSynthVoiceIteratorGetStatus(iterator);
		status->state = 0;
		status->keyOnCount = param->keyOnCount;
	}
}

SceSasAsSynthVoiceParam *sceSasAsSynthVoiceIteratorGetParam(SceSasAsSynthVoiceIterator *iterator)
{
	return (SceSasAsSynthVoiceParam *)((SceUInt8 *)iterator->info->params +
					   iterator->info->paramStride * iterator->index);
}

SceSasAsSynthVoiceWork *sceSasAsSynthVoiceIteratorGetWork(SceSasAsSynthVoiceIterator *iterator)
{
	return (SceSasAsSynthVoiceWork *)((SceUInt8 *)iterator->info->works +
					  iterator->info->workStride * iterator->index);
}

SceSasAsSynthVoiceStatus *sceSasAsSynthVoiceIteratorGetStatus(SceSasAsSynthVoiceIterator *iterator)
{
	return &iterator->info->status[iterator->index];
}

/* where a source's samples are: they are in memory as they are */
__attribute__((noinline, noclone))
const void *sceSasAsSynthVoiceMapSource(SceSasAsSynthVoiceIterator *iterator, SceUIntPtr address,
					SceSize size, SceSize stride, SceSize *mapped)
{
	if (mapped != NULL)
		*mapped = size;
	return (const void *)address;
}

/* a buss's channel buffers, marked mixed into; used says whether something already was */
SceInt32 **sceSasAsSynthVoiceIteratorGetBuss(SceSasAsSynthVoiceIterator *iterator, SceUInt32 buss,
					     SceUInt8 *used)
{
	const SceSasAsRenderInfo *render = iterator->render;
	SceUInt32 *word, bits;
	SceInt32 **buffers;

	if (buss >= render->numBusses)
		return NULL;
	word = &iterator->usedBusses[buss >> 5];
	bits = *word;
	if (used != NULL)
		*used = (bits >> (buss & 31)) & 1;
	buffers = (SceInt32 **)render->bussBuffers + buss * render->numChannels;
	*word = bits | (1 << (buss & 31));
	return buffers;
}

static inline SceInt32 sceSasAsSynthClamp16(SceInt32 value)
{
	if (value >= 32767)
		value = 32767;
	if (value < -32768)
		value = -32768;
	return value;
}

/* the noise's clock: its divider and the pattern of the ticks it takes */
static inline void sceSasAsSynthVoiceStartNoise(SceSasAsSynthVoiceWork *work)
{
	SceUInt32 noise = work->source.noise;

	work->phase = 0xffffffff;
	work->phaseStep = 0x4000 >> (noise >> 2);
	if (work->phaseStep != 0)
		work->phase = (SceUInt8)(0xffffeeaa >> ((noise & 3) * 4)) * 0x01010101;
	else
		work->phaseStep = 1;
	work->noise = 0;
	work->noiseCount = 0;
}

static inline void sceSasAsSynthVoiceStartSource(SceSasAsSynthVoiceWork *work)
{
	switch (work->source.type) {
	case SCE_SAS_AS_SYNTH_SOURCE_SINE:
	case SCE_SAS_AS_SYNTH_SOURCE_SAW:
	case SCE_SAS_AS_SYNTH_SOURCE_SAW_DOWN:
	case SCE_SAS_AS_SYNTH_SOURCE_TRIANGLE:
	case SCE_SAS_AS_SYNTH_SOURCE_SQUARE:
		work->noise = 0;
		work->noiseCount = 0;
		work->phaseStep = ((work->source.oscillator.frequency << 16) /
				   SCE_SAS_AS_SAMPLE_RATE) << 16;
		work->phase = ((work->source.oscillator.phase << 16) / 360) << 16;
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_WHITE_NOISE:
		work->phase = 0;
		work->phaseStep = 0;
		work->noise = 0;
		work->noiseCount = 0;
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_NOISE:
		sceSasAsSynthVoiceStartNoise(work);
		break;
	}
}

/* takes what an update changed in the source playing, when it is of the same type */
static inline void sceSasAsSynthVoiceReloadSource(const SceSasAsSynthVoiceParam *param,
						  SceSasAsSynthVoiceWork *work)
{
	const SceSasAsSynthSource *source;

	if (work->sourceIndex < 0)
		return;
	source = (const SceSasAsSynthSource *)SCE_SAS_AS_AT(param->sourcesOffset) +
		 work->sourceIndex;
	if (source->type != work->source.type)
		return;
	work->source.flags = source->flags;
	work->source.loopCount = source->loopCount;
	work->source.next = source->next;
	work->source.length = source->length;
	switch (source->type) {
	case SCE_SAS_AS_SYNTH_SOURCE_SINE:
	case SCE_SAS_AS_SYNTH_SOURCE_SAW:
	case SCE_SAS_AS_SYNTH_SOURCE_SAW_DOWN:
	case SCE_SAS_AS_SYNTH_SOURCE_TRIANGLE:
	case SCE_SAS_AS_SYNTH_SOURCE_SQUARE:
	case SCE_SAS_AS_SYNTH_SOURCE_WHITE_NOISE:
		work->source.oscillator.frequency = source->oscillator.frequency;
		work->phaseStep = ((work->source.oscillator.frequency << 16) /
				   SCE_SAS_AS_SAMPLE_RATE) << 16;
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_NOISE:
		work->source.noise = source->noise;
		sceSasAsSynthVoiceStartNoise(work);
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_PCM8:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM8_SIGNED:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM16:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM16_SWAPPED:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM24:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM24_SWAPPED:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM32:
	case SCE_SAS_AS_SYNTH_SOURCE_PCM32_SWAPPED:
	case SCE_SAS_AS_SYNTH_SOURCE_FLOAT:
	case SCE_SAS_AS_SYNTH_SOURCE_FLOAT_SWAPPED:
	case SCE_SAS_AS_SYNTH_SOURCE_ADPCM:
	case SCE_SAS_AS_SYNTH_SOURCE_ADPCM_SHORT:
		work->source.pcm.address = source->pcm.address;
		break;
	}
}

/* the oscillators, from the phase */
static inline void sceSasAsSynthVoiceOscillate(SceSasAsSynthVoiceWork *work, SceInt16 *samples,
					       SceUInt32 count)
{
	SceUInt32 step, phase, index;
	SceInt32 value, next;

	if (samples == NULL)
		return;
	step = work->phaseStep;
	phase = work->phase;
	switch (work->source.type) {
	case SCE_SAS_AS_SYNTH_SOURCE_SAW:
		for (; count != 0; count--) {
			*samples++ = phase >> 16;
			phase += step;
		}
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_SAW_DOWN:
		for (; count != 0; count--) {
			*samples++ = -(phase >> 16);
			phase += step;
		}
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_TRIANGLE:
		for (; count != 0; count--) {
			value = phase >> 15;
			if (phase < 0x40000000 || phase >= 0xc0000000) {
				*samples++ = value;
			} else {
				value = 0x10000 - value;
				if (value >= 32767)
					value = 32767;
				*samples++ = value;
			}
			phase += step;
		}
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_SQUARE:
		for (; count != 0; count--) {
			*samples++ = (SceInt32)phase < 0 ? -32768 : 32767;
			phase += step;
		}
		break;
	case SCE_SAS_AS_SYNTH_SOURCE_WHITE_NOISE:
		for (; count != 0; count--) {
			phase = phase * 1664525 + 1;
			*samples++ = phase >> 12;
			phase += step;
		}
		break;
	default:
		for (; count != 0; count--) {
			index = phase >> 22;
			if (phase < 0x40000000) {
				value = sceSasAsSynthSineTable[index];
				next = sceSasAsSynthSineTable[index + 1];
			} else if (phase & 0x80000000) {
				if (phase < 0xc0000000) {
					value = -sceSasAsSynthSineTable[index - 512];
					next = -sceSasAsSynthSineTable[index - 511];
				} else {
					value = -sceSasAsSynthSineTable[1024 - index];
					next = -sceSasAsSynthSineTable[1023 - index];
				}
			} else {
				value = sceSasAsSynthSineTable[512 - index];
				next = sceSasAsSynthSineTable[511 - index];
			}
			*samples++ = value + ((SceInt32)(((phase >> 8) & 0x3fff) * (next - value)) >> 14);
			phase += step;
		}
		break;
	}
	work->phase = phase;
}

/* the noise: a shift register, shifted at the clock's ticks the pattern takes */
static inline void sceSasAsSynthVoiceNoise(SceSasAsSynthVoiceWork *work, SceInt16 *samples,
					   SceUInt32 count)
{
	SceUInt32 pattern, value;
	SceInt32 period, counter;

	if (samples == NULL)
		return;
	pattern = work->phase;
	period = work->phaseStep;
	value = work->noise;
	counter = work->noiseCount;
	for (; count != 0; count--) {
		*samples++ = value;
		if (--counter < 0) {
			pattern = (pattern >> 1) | (pattern << 31);
			counter = period;
			if ((SceInt32)pattern < 0) {
				value <<= 1;
				value += ((0x69 >> ((value >> 10) & 7)) ^ (value >> 15)) & 1;
			}
		}
	}
	work->phase = pattern;
	work->noise = value;
	work->noiseCount = counter;
}

static inline SceInt32 sceSasAsSynthNibble(SceUInt32 byte, SceUInt32 shift)
{
	return (SceInt32)(byte << 28) >> shift;
}

static inline SceInt32 sceSasAsSynthHighNibble(SceUInt32 byte, SceUInt32 shift)
{
	return (SceInt32)((byte & 0xf0) << 24) >> shift;
}

/* a float sample (1.0 at full scale) as 16 bits: Sony's clamp below is -524303.0, not -32768.0 */
static inline SceInt16 sceSasAsSynthFloatToSample(float value)
{
	value *= 32768.0f;
	if (value < -524303.0f)
		return -32768;
	if (value > 32767.0f)
		return 32767;
	return (SceInt32)value;
}

/*
 * Plays the voice's sources into samples, count of them at its pitch: the samples the
 * resampler needs are generated after the eight it kept, and resampled.
 */
void sceSasAsSynthVoiceGenerate(SceSasAsSynthVoiceIterator *iterator,
				const SceSasAsSynthVoiceParam *param, SceSasAsSynthVoiceWork *work,
				SceInt16 *buffer, SceInt16 *samples, SceUInt32 count)
{
	const SceSasAsSynthSource *sources = SCE_SAS_AS_AT(param->sourcesOffset);
	const SceSasAsSynthTaps *taps, *tail;
	SceInt16 *output, *write;
	const SceInt16 *input;
	SceUInt32 needed, done, length, position, size, numBlocks, block, blockPosition, shift;
	SceUInt32 fraction, next, numLoops, i, n;
	SceInt32 sum, h0, h1, h2, h3, s0, s1, s2, s3;
	SceSize mapped;
	const SceUInt8 *bytes;
	const SceInt16 *coefficients;
	SceInt8 nextSource;
	SceUInt8 header, flags;
	union {
		SceUInt32 bits;
		float value;
	} swapped;
	SceInt32 pitch;

	if (param->updated & SCE_SAS_AS_SYNTH_UPDATED_SOURCES)
		sceSasAsSynthVoiceReloadSource(param, work);

	/* the samples kept from the last ADPCM block first */
	needed = (work->fraction + param->pitch * count) >> 14;
	output = buffer + 8;
	done = 0;
	if (work->numDecoded != 0) {
		n = needed < work->numDecoded ? needed : work->numDecoded;
		output = buffer + 8 + n;
		sceSasAsMemcpy(buffer + 8,
			       &work->decoded[SCE_SAS_AS_SYNTH_ADPCM_SAMPLES - work->numDecoded],
			       n * sizeof(SceInt16));
		work->numDecoded -= n;
		done = n;
	}

	numLoops = 0;
	while (done < needed) {
		if (work->sourceIndex < 0) {
			/* the first source */
			work->sourceIndex = 0;
			work->position = 0;
			work->loopStart = -1;
			work->loopEnd = -1;
			work->source = sources[0];
			sceSasAsSynthVoiceStartSource(work);
		} else if (work->loopEnd >= 0 && (SceUInt32)work->loopEnd <= work->position) {
			if (work->loopStart < 0) {
				work->flags = 0;
				break;
			}
			work->position = work->loopStart;
		} else if (work->position >= work->source.length) {
			/* the source played: again, or the next one */
			if (work->source.length == 0)
				break;
			if (work->source.loopCount != 0) {
				if (work->source.loopCount != SCE_SAS_AS_SYNTH_SOURCE_LOOP_FOREVER)
					work->source.loopCount--;
				work->position = 0;
			} else {
				if (numLoops >= param->maxSources)
					break;
				nextSource = sources[work->sourceIndex].next;
				if (nextSource == SCE_SAS_AS_SYNTH_SOURCE_NEXT_WAIT) {
					work->flags |= SCE_SAS_AS_SYNTH_VOICE_WAITING;
					break;
				}
				if (nextSource == SCE_SAS_AS_SYNTH_SOURCE_NEXT_STOP) {
					work->flags = 0;
					break;
				}
				if (nextSource < 0 || nextSource >= param->maxSources) {
					work->error = SCE_SAS_AS_ERROR_CONFIG;
					break;
				}
				work->sourceIndex = nextSource;
				work->position = 0;
				work->source = sources[nextSource];
				sceSasAsSynthVoiceStartSource(work);
				if (work->source.length != 0)
					numLoops = 0;
				else
					numLoops++;
			}
		}

		position = work->position;
		length = work->source.length - position;
		n = needed - done;
		if (length < n)
			n = length;
		work->flags = (work->flags & ~SCE_SAS_AS_SYNTH_VOICE_WAITING) |
			      SCE_SAS_AS_SYNTH_VOICE_PLAYING;
		switch (work->source.type) {
		case SCE_SAS_AS_SYNTH_SOURCE_SILENCE:
			if (n != 0)
				memset(output, 0, n * sizeof(SceInt16));
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_SINE:
		case SCE_SAS_AS_SYNTH_SOURCE_SAW:
		case SCE_SAS_AS_SYNTH_SOURCE_SAW_DOWN:
		case SCE_SAS_AS_SYNTH_SOURCE_TRIANGLE:
		case SCE_SAS_AS_SYNTH_SOURCE_SQUARE:
		case SCE_SAS_AS_SYNTH_SOURCE_WHITE_NOISE:
			sceSasAsSynthVoiceOscillate(work, output, n);
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_NOISE:
			sceSasAsSynthVoiceNoise(work, output, n);
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_PCM8:
		case SCE_SAS_AS_SYNTH_SOURCE_PCM8_SIGNED:
			size = work->source.numChannels;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * (work->source.pcm.start + position),
							    size * n, size, &mapped);
			n = mapped / size;
			if (bytes == NULL)
				break;
			write = output;
			bytes += work->source.channel;
			if (work->source.type == SCE_SAS_AS_SYNTH_SOURCE_PCM8) {
				for (i = n; i != 0; i--) {
					*write++ = (*bytes - 128) << 8;
					bytes += work->source.numChannels;
				}
			} else {
				for (i = n; i != 0; i--) {
					*write++ = *(const SceInt8 *)bytes << 8;
					bytes += work->source.numChannels;
				}
			}
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_PCM16:
		case SCE_SAS_AS_SYNTH_SOURCE_PCM16_SWAPPED:
			size = work->source.numChannels * 2;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * (work->source.pcm.start + position),
							    size * n, size, &mapped);
			n = mapped / size;
			if (bytes == NULL)
				break;
			write = output;
			input = (SceInt16 *)bytes + work->source.channel;
			if (work->source.type == SCE_SAS_AS_SYNTH_SOURCE_PCM16) {
				for (i = n; i != 0; i--) {
					*write++ = *input;
					write += work->source.numChannels;
				}
			} else {
				for (i = n; i != 0; i--) {
					*write++ = __builtin_bswap16(*input);
					write += work->source.numChannels;
				}
			}
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_PCM24:
		case SCE_SAS_AS_SYNTH_SOURCE_PCM24_SWAPPED:
			size = work->source.numChannels * 3;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * (work->source.pcm.start + position),
							    size * n, size, &mapped);
			n = mapped / size;
			if (bytes == NULL)
				break;
			write = output;
			bytes += work->source.channel * 3;
			if (work->source.type == SCE_SAS_AS_SYNTH_SOURCE_PCM24) {
				for (i = n; i != 0; i--) {
					*write++ = bytes[1] | (bytes[2] << 8);
					bytes += size;
				}
			} else {
				for (i = n; i != 0; i--) {
					*write++ = __builtin_bswap16(*(const SceUInt16 *)bytes);
					bytes += work->source.numChannels * 3;
				}
			}
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_PCM32:
		case SCE_SAS_AS_SYNTH_SOURCE_PCM32_SWAPPED:
			size = work->source.numChannels * 4;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * (work->source.pcm.start + position),
							    size * n, size, &mapped);
			n = mapped / size;
			if (bytes == NULL)
				break;
			write = output;
			bytes += work->source.channel * 4;
			if (work->source.type == SCE_SAS_AS_SYNTH_SOURCE_PCM32) {
				for (i = n; i != 0; i--) {
					*write++ = ((const SceInt16 *)bytes)[1];
					bytes += work->source.numChannels * 4;
				}
			} else {
				for (i = n; i != 0; i--) {
					*write++ = __builtin_bswap32(*(const SceUInt32 *)bytes) >> 16;
					bytes += work->source.numChannels * 4;
				}
			}
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_FLOAT:
		case SCE_SAS_AS_SYNTH_SOURCE_FLOAT_SWAPPED:
			size = work->source.numChannels * 4;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * (work->source.pcm.start + position),
							    size * n, size, &mapped);
			n = mapped / size;
			if (bytes == NULL)
				break;
			write = output;
			bytes += work->source.channel * 4;
			if (work->source.type == SCE_SAS_AS_SYNTH_SOURCE_FLOAT) {
				for (i = n; i != 0; i--) {
					*write++ = sceSasAsSynthFloatToSample(*(const float *)bytes);
					bytes += work->source.numChannels * 4;
				}
			} else {
				for (i = n; i != 0; i--) {
					swapped.bits = __builtin_bswap32(*(const SceUInt32 *)bytes);
					*write++ = sceSasAsSynthFloatToSample(swapped.value);
					bytes += work->source.numChannels * 4;
				}
			}
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_ADPCM:
			numBlocks = (n + SCE_SAS_AS_SYNTH_ADPCM_SAMPLES - 1) / SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
			size = work->source.numChannels * 16;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * ((work->source.pcm.start + position) /
								    SCE_SAS_AS_SYNTH_ADPCM_SAMPLES),
							    size * numBlocks, size, &mapped);
			n = numBlocks * SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
			if (bytes == NULL)
				break;
			write = output;
			h0 = work->history[0];
			h1 = work->history[1];
			h2 = work->history[2];
			h3 = work->history[3];
			bytes += work->source.channel * 16;
			blockPosition = work->position;
			for (block = 0; block < numBlocks; block++) {
				header = bytes[0];
				flags = bytes[1];
				shift = (header & 0xf) + 16;
				coefficients = sceSasAsSynthAdpcmTable[(flags & 0xf0) | (header >> 4)];
				for (i = 2; i < 16; i += 2) {
					s0 = sceSasAsSynthNibble(bytes[i], shift) +
					     ((coefficients[3] * h3 + coefficients[2] * h2 +
					       coefficients[0] * h0 + coefficients[1] * h1) >> 13);
					s0 = sceSasAsSynthClamp16(s0);
					s1 = sceSasAsSynthHighNibble(bytes[i], shift) +
					     ((coefficients[3] * h2 + coefficients[2] * h1 +
					       coefficients[1] * h0 + coefficients[0] * s0) >> 13);
					s1 = sceSasAsSynthClamp16(s1);
					s2 = sceSasAsSynthNibble(bytes[i + 1], shift) +
					     ((coefficients[3] * h1 + coefficients[2] * h0 +
					       coefficients[1] * s0 + coefficients[0] * s1) >> 13);
					s2 = sceSasAsSynthClamp16(s2);
					s3 = sceSasAsSynthHighNibble(bytes[i + 1], shift) +
					     ((coefficients[3] * h0 + coefficients[2] * s0 +
					       coefficients[1] * s1 + coefficients[0] * s2) >> 13);
					s3 = sceSasAsSynthClamp16(s3);
					write[0] = s0;
					write[1] = s1;
					write[2] = s2;
					write[3] = s3;
					write += 4;
					h3 = s0;
					h2 = s1;
					h1 = s2;
					h0 = s3;
				}
				/* the block's loop marks: an end, an end after it, a loop start, a loop end */
				switch (flags & 0xf) {
				case 7:
					work->loopStart = -1;
					n = block * SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
					work->loopEnd = n + work->position;
					goto adpcmEnd;
				case 1:
					work->loopStart = -1;
					n = (block + 1) * SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
					work->loopEnd = n + work->position;
					goto adpcmEnd;
				case 6:
					if (work->source.flags & SCE_SAS_AS_SYNTH_SOURCE_LOOP)
						work->loopStart = blockPosition;
					else
						work->loopStart = -1;
					break;
				case 3:
					if (work->source.flags & SCE_SAS_AS_SYNTH_SOURCE_LOOP) {
						n = (block + 1) * SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
						work->loopEnd = n + work->position;
						goto adpcmEnd;
					}
					work->loopEnd = -1;
					break;
				}
				blockPosition += SCE_SAS_AS_SYNTH_ADPCM_SAMPLES;
				bytes += size;
			}
adpcmEnd:
			work->history[0] = h0;
			work->history[1] = h1;
			work->history[2] = h2;
			work->history[3] = h3;
			break;
		case SCE_SAS_AS_SYNTH_SOURCE_ADPCM_SHORT:
			numBlocks = (n + SCE_SAS_AS_SYNTH_ADPCM_SHORT_SAMPLES - 1) /
				    SCE_SAS_AS_SYNTH_ADPCM_SHORT_SAMPLES;
			size = work->source.numChannels * 4;
			bytes = sceSasAsSynthVoiceMapSource(iterator, work->source.pcm.address +
							    size * ((position + work->source.pcm.start) /
								    SCE_SAS_AS_SYNTH_ADPCM_SHORT_SAMPLES),
							    size * numBlocks, size, &mapped);
			n = numBlocks * SCE_SAS_AS_SYNTH_ADPCM_SHORT_SAMPLES;
			if (bytes == NULL)
				break;
			write = output;
			h0 = work->history[0];
			h1 = work->history[1];
			bytes += work->source.channel * 4;
			for (block = 0; block < numBlocks; block++) {
				header = bytes[0];
				shift = (header & 0xf) + 16;
				coefficients = sceSasAsSynthAdpcmShortTable[header >> 4];
				for (i = 1; i < 4; i++) {
					s0 = sceSasAsSynthNibble(bytes[i], shift) +
					     ((coefficients[0] * h0 + coefficients[1] * h1) >> 13);
					s0 = sceSasAsSynthClamp16(s0);
					s1 = sceSasAsSynthHighNibble(bytes[i], shift) +
					     ((coefficients[1] * h0 + coefficients[0] * s0) >> 13);
					s1 = sceSasAsSynthClamp16(s1);
					write[0] = s0;
					write[1] = s1;
					write += 2;
					h1 = s0;
					h0 = s1;
				}
				bytes += size;
			}
			work->history[0] = h0;
			work->history[1] = h1;
			break;
		default:
			work->flags = (work->flags & ~(SCE_SAS_AS_SYNTH_VOICE_PLAYING |
						       SCE_SAS_AS_SYNTH_VOICE_WAITING |
						       SCE_SAS_AS_SYNTH_VOICE_ERROR)) |
				      SCE_SAS_AS_SYNTH_VOICE_PLAYING | SCE_SAS_AS_SYNTH_VOICE_ERROR;
			work->error = SCE_SAS_AS_ERROR_NOT_FOUND;
			goto generated;
		}

		/* what the source had left of what was made counts */
		length = work->source.length - work->position;
		if (length >= n)
			length = n;
		done += length;
		work->time += length;
		work->position += length;
		output += length;
	}

generated:
	if (done > needed) {
		/* an ADPCM block's samples not needed yet */
		n = done - needed;
		sceSasAsMemcpy(&work->decoded[SCE_SAS_AS_SYNTH_ADPCM_SAMPLES - n], output - n,
			       n * sizeof(SceInt16));
		work->numDecoded = n;
	} else if (done < needed) {
		sceSasAsSynthGain(output, NULL, needed - done, 0, 0);
	}

	/* resampled at the pitch through the 8-tap filter */
	fraction = work->fraction;
	pitch = param->pitch;
	sceSasAsMemcpy(buffer, work->resampleHistory, sizeof(work->resampleHistory));
	if (pitch == 0x4000 && fraction == 0) {
		input = buffer;
		buffer += count;
		sceSasAsMemcpy(samples, input, count * sizeof(SceInt16));
	} else {
		for (i = count; i != 0; i--) {
			taps = &sceSasAsSynthResampleTable[fraction >> 5];
			tail = &sceSasAsSynthResampleTable[512 - (fraction >> 5)];
			next = fraction + pitch;
			sum = buffer[3] * (*taps)[3] + buffer[2] * (*taps)[2] +
			      buffer[0] * (*taps)[0] + buffer[1] * (*taps)[1];
			sum = buffer[4] * (*tail)[3] + sum;
			fraction = next & 0x3fff;
			sum = (buffer[7] * (*tail)[0] + buffer[6] * (*tail)[1] +
			       buffer[5] * (*tail)[2] + sum) >> 14;
			buffer += next >> 14;
			*samples++ = sceSasAsSynthClamp16(sum);
		}
		work->fraction = fraction;
	}
	sceSasAsMemcpy(work->resampleHistory, buffer, sizeof(work->resampleHistory));
}

/* soft clipping: linear to a third of full scale, a parabola to two thirds, then flat */
void sceSasAsSynthVoiceDistort(const SceSasAsSynthVoiceParam *param, SceSasAsSynthVoiceWork *work,
			       SceInt16 *samples, SceUInt32 count)
{
	SceInt16 gain = param->distortionGain;
	SceInt16 level = param->distortionLevel;
	SceInt32 sample, magnitude, curve;

	for (; count != 0; count--) {
		sample = (*samples * gain) >> 10;
		magnitude = __builtin_abs(sample);
		if (magnitude <= 10921) {
			curve = sample * 2;
		} else if (magnitude <= 21844) {
			curve = 0x10000 - magnitude * 3;
			curve = 0x18000 - ((5461 * ((curve * curve) >> 16)) >> 14);
			if (sample < 0)
				curve = -curve;
		} else {
			curve = sample < 0 ? -32768 : 32767;
		}
		*samples++ = sceSasAsSynthClamp16((level * curve) >> 12);
	}
}

/* the ADSR envelope over samples, stage by stage; a stage's end moves the voice on */
void sceSasAsSynthVoiceEnvelope(const SceSasAsSynthVoiceParam *param, SceSasAsSynthVoiceWork *work,
				SceInt16 *samples, SceInt32 count)
{
	SceInt32 level = work->level;
	SceInt32 rate, target, scale;
	SceUInt32 stage, next, mode;

	if ((param->events & SCE_SAS_AS_SYNTH_KEY_OFF) &&
	    work->stage <= SCE_SAS_AS_SYNTH_ENVELOPE_HOLD)
		work->stage = SCE_SAS_AS_SYNTH_ENVELOPE_RELEASE;
	if (count <= 0)
		goto done;
	stage = work->stage;
	for (;;) {
		switch (stage) {
		case SCE_SAS_AS_SYNTH_ENVELOPE_ATTACK:
			mode = param->adsrModes.attack;
			rate = param->attackRate;
			if (mode != SCE_SAS_AS_SYNTH_CURVE_LINEAR_UP &&
			    mode != SCE_SAS_AS_SYNTH_CURVE_BENT_UP &&
			    mode != SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_UP)
				goto invalid;
			target = SCE_SAS_AS_SYNTH_ENVELOPE_ONE;
			next = SCE_SAS_AS_SYNTH_ENVELOPE_DECAY;
			break;
		case SCE_SAS_AS_SYNTH_ENVELOPE_DECAY:
			mode = param->adsrModes.decay;
			rate = param->decayRate;
			target = param->sustainLevel;
			if (mode != SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_DOWN &&
			    mode != SCE_SAS_AS_SYNTH_CURVE_LINEAR_DOWN)
				goto invalid;
			next = SCE_SAS_AS_SYNTH_ENVELOPE_SUSTAIN;
			break;
		case SCE_SAS_AS_SYNTH_ENVELOPE_SUSTAIN:
			mode = param->adsrModes.sustain;
			rate = param->sustainRate;
			if (mode == SCE_SAS_AS_SYNTH_CURVE_LINEAR_DOWN ||
			    mode == SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_DOWN) {
				target = 0;
				next = SCE_SAS_AS_SYNTH_ENVELOPE_END;
			} else {
				target = SCE_SAS_AS_SYNTH_ENVELOPE_ONE;
				next = SCE_SAS_AS_SYNTH_ENVELOPE_HOLD;
			}
			break;
		case SCE_SAS_AS_SYNTH_ENVELOPE_HOLD:
			rate = 0;
			goto constant;
		case SCE_SAS_AS_SYNTH_ENVELOPE_RELEASE:
			mode = param->adsrModes.release;
			rate = param->releaseRate;
			if (mode != SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_DOWN &&
			    mode != SCE_SAS_AS_SYNTH_CURVE_LINEAR_DOWN)
				goto invalid;
			target = 0;
			next = SCE_SAS_AS_SYNTH_ENVELOPE_END;
			break;
		default:
			goto silence;
		}

		switch (mode) {
		case SCE_SAS_AS_SYNTH_CURVE_LINEAR_DOWN:
			for (;;) {
				level -= rate;
				if (level <= target)
					break;
				*samples = (*samples * (level >> 16)) >> 14;
				samples++;
				if (--count == 0)
					goto done;
			}
			if (level < target)
				level = target;
			break;
		case SCE_SAS_AS_SYNTH_CURVE_LINEAR_UP:
			for (;;) {
				level += rate;
				if (level >= target)
					break;
				*samples = (*samples * (level >> 16)) >> 14;
				samples++;
				if (--count == 0)
					goto done;
			}
			if (level >= target)
				level = target;
			break;
		case SCE_SAS_AS_SYNTH_CURVE_BENT_UP:
			for (;;) {
				level += rate >> (level > 0x30000000 ? 2 : 0);
				if (level >= target)
					break;
				*samples = (*samples * (level >> 16)) >> 14;
				samples++;
				if (--count == 0)
					goto done;
			}
			if (level >= SCE_SAS_AS_SYNTH_ENVELOPE_ONE)
				level = SCE_SAS_AS_SYNTH_ENVELOPE_ONE;
			break;
		case SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_DOWN:
			for (;;) {
				level = ((SceInt64)level * (SceUInt32)-rate) >> 32;
				if (level <= target)
					break;
				*samples = (*samples * (level >> 16)) >> 14;
				samples++;
				if (--count == 0)
					goto done;
			}
			if (level < target)
				level = target;
			break;
		case SCE_SAS_AS_SYNTH_CURVE_EXPONENTIAL_UP:
			/* Sony's test ends the stage while the level is at or below the target */
			for (;;) {
				level += (SceInt32)(((SceInt64)(SCE_SAS_AS_SYNTH_ENVELOPE_ONE - level) *
						     rate) >> 32) + 0x4000;
				if (level <= target)
					break;
				*samples = (*samples * (level >> 16)) >> 14;
				samples++;
				if (--count == 0)
					goto done;
			}
			if (level >= target)
				level = target;
			break;
		case SCE_SAS_AS_SYNTH_CURVE_DIRECT:
			goto constant;
		default:
			work->error = SCE_SAS_AS_ERROR_CONFIG;
			goto silence;
		}
		if (next == SCE_SAS_AS_SYNTH_ENVELOPE_END)
			goto silence;
		stage = next;
		work->stage = stage;
		if (count <= 0)
			goto done;
	}

invalid:
	if (mode != SCE_SAS_AS_SYNTH_CURVE_DIRECT) {
		work->error = SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
		goto silence;
	}
constant:
	/* the level is the rate from here on */
	if (level != SCE_SAS_AS_SYNTH_ENVELOPE_ONE) {
		scale = rate >> 16;
		for (; count != 0; count--) {
			*samples = (scale * *samples) >> 14;
			samples++;
		}
		level = rate;
	}
done:
	work->level = level;
	return;

silence:
	memset(samples, 0, count * sizeof(SceInt16));
	work->level = 0;
	work->flags = 0;
}

void sceSasAsSynthVoiceUnused(void)
{
}

static inline SceInt16 sceSasAsSynthCoefficient(float value)
{
	if (value < -32768.0f)
		return -32768;
	if (value > 32767.0f)
		return 32767;
	return (SceInt32)value;
}

/* a biquad's coefficients (4.12), normalized by a0, the feedback ones negated */
static inline void sceSasAsSynthFilterSet(SceInt16 *design, float b0, float b1, float b2, float a0,
					  float a1, float a2)
{
	float scale = 1.0f / a0;

	design[0] = sceSasAsSynthCoefficient(b0 * scale * 4096.0f);
	design[1] = sceSasAsSynthCoefficient(b1 * scale * 4096.0f);
	design[2] = sceSasAsSynthCoefficient(b2 * scale * 4096.0f);
	design[3] = sceSasAsSynthCoefficient(-(a1 * scale) * 4096.0f);
	design[4] = sceSasAsSynthCoefficient(-(a2 * scale) * 4096.0f);
}

/*
 * A filter's coefficients from its frequency (Hz), resonance (2.14) and gain (4.12), by the
 * Audio EQ Cookbook: low-pass, high-pass, notch, the band-passes, the shelves, the peak, and
 * two first-order filters.
 */
void sceSasAsSynthFilterDesign(SceUInt32 type, SceUInt32 sampleRate, const SceInt16 *coefficients,
			       SceInt16 *design)
{
	float frequency, resonance, gain, q, amplitude, omega, sine, cosine, alpha, beta;

	if (type == SCE_SAS_AS_SYNTH_FILTER_COEFFICIENTS) {
		design[0] = coefficients[0];
		design[1] = coefficients[1];
		design[2] = coefficients[2];
		design[3] = coefficients[3];
		design[4] = coefficients[4];
		return;
	}
	frequency = (float)coefficients[0] / (float)sampleRate;
	frequency = frequency < 0.0005f ? 0.0005f : frequency > 0.4995f ? 0.4995f : frequency;
	resonance = 1.0f - (float)coefficients[1] / 16384.0f;
	gain = (float)coefficients[2] / 4096.0f;
	q = resonance > 0.0001f ? 0.5f / resonance : 5000.0f;
	gain = gain > 0.0001f ? gain : 0.0001f;
	amplitude = sceSasAsSqrtf(gain);
	omega = frequency * SCE_SAS_AS_TWO_PI;
	sine = sceSasAsSinf(omega);
	cosine = sceSasAsCosf(omega);
	alpha = sine / (q + q);
	if (type >= 6 && type <= 8)
		beta = sceSasAsSqrtf(amplitude) / q;
	else
		beta = 0.0f;
	switch (type) {
	case 1:	/* low-pass */
		sceSasAsSynthFilterSet(design, (1.0f - cosine) * 0.5f, 1.0f - cosine,
				       (1.0f - cosine) * 0.5f, 1.0f + alpha, -2.0f * cosine,
				       1.0f - alpha);
		break;
	case 2:	/* high-pass */
		sceSasAsSynthFilterSet(design, (1.0f + cosine) * 0.5f, -(1.0f + cosine),
				       (1.0f + cosine) * 0.5f, 1.0f + alpha, -2.0f * cosine,
				       1.0f - alpha);
		break;
	case 3:	/* notch */
		sceSasAsSynthFilterSet(design, 1.0f, -2.0f * cosine, 1.0f, 1.0f + alpha,
				       -2.0f * cosine, 1.0f - alpha);
		break;
	case 4:	/* band-pass, constant skirt gain */
		sceSasAsSynthFilterSet(design, sine * 0.5f, 0.0f, -sine * 0.5f, 1.0f + alpha,
				       -2.0f * cosine, 1.0f - alpha);
		break;
	case 5:	/* band-pass, constant peak gain */
		sceSasAsSynthFilterSet(design, alpha, 0.0f, -alpha, 1.0f + alpha, -2.0f * cosine,
				       1.0f - alpha);
		break;
	case 6:	/* low shelf */
		sceSasAsSynthFilterSet(design,
			amplitude * ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine + beta * sine),
			2.0f * amplitude * ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine),
			amplitude * ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine - beta * sine),
			(amplitude + 1.0f) + (amplitude - 1.0f) * cosine + beta * sine,
			-2.0f * ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine),
			(amplitude + 1.0f) + (amplitude - 1.0f) * cosine - beta * sine);
		break;
	case 7:	/* high shelf */
		sceSasAsSynthFilterSet(design,
			amplitude * ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine + beta * sine),
			-2.0f * amplitude * ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine),
			amplitude * ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine - beta * sine),
			(amplitude + 1.0f) - (amplitude - 1.0f) * cosine + beta * sine,
			2.0f * ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine),
			(amplitude + 1.0f) - (amplitude - 1.0f) * cosine - beta * sine);
		break;
	case 8:	/* peak */
		sceSasAsSynthFilterSet(design, 1.0f + alpha * amplitude, -2.0f * cosine,
				       1.0f - alpha * amplitude, 1.0f + alpha / amplitude,
				       -2.0f * cosine, 1.0f - alpha / amplitude);
		break;
	case 9:	/* first order */
		sceSasAsSynthFilterSet(design, sine, sine, 0.0f, sine + cosine + 1.0f,
				       sine - cosine - 1.0f, 0.0f);
		break;
	case 10:	/* first order */
		sceSasAsSynthFilterSet(design, cosine, -cosine - 1.0f, 0.0f, sine + cosine + 1.0f,
				       sine - cosine - 1.0f, 0.0f);
		break;
	default:
		sceSasAsSynthFilterSet(design, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
}

/*
 * The voice's filters on send (0: the voice itself) from input to output, each designed
 * again when an update changed them: 1 if one ran.
 */
int sceSasAsSynthVoiceFilter(const SceSasAsSynthVoiceParam *param, SceSasAsSynthVoiceWork *work,
			     SceUInt8 send, SceUInt32 sampleRate, const SceInt16 *input,
			     SceInt16 *output, SceUInt32 count)
{
	SceUInt32 mask = param->filterMask;
	const SceUInt8 *types = SCE_SAS_AS_AT(param->filterTypesOffset);
	const SceInt16 *in;
	SceInt16 *design, *state, *out;
	SceInt32 x, x1, x2, y, y1, y2;
	SceUInt32 index, i;
	int ran;

	if (mask == 0)
		return 0;
	ran = 0;
	for (index = 0; mask != 0; mask >>= 1, index++, types += 2) {
		if (!(mask & 1) || types[1] != send)
			continue;
		design = (SceInt16 *)SCE_SAS_AS_AT(work->filterDesignOffset) + index * 5;
		state = (SceInt16 *)SCE_SAS_AS_AT(work->filterStateOffset) + index * 4;
		if (work->filterReady & (1 << index)) {
			if (param->updated & SCE_SAS_AS_SYNTH_UPDATED_FILTERS)
				sceSasAsSynthFilterDesign(types[0], sampleRate,
					(const SceInt16 *)SCE_SAS_AS_AT(param->filterCoefficientsOffset) +
					index * 5, design);
		} else {
			sceSasAsMemZero(state, 4 * sizeof(SceInt16));
			sceSasAsSynthFilterDesign(types[0], sampleRate,
				(const SceInt16 *)SCE_SAS_AS_AT(param->filterCoefficientsOffset) +
				index * 5, design);
			work->filterReady |= 1 << index;
		}
		x1 = state[0];
		x2 = state[1];
		y1 = state[2];
		y2 = state[3];
		in = input;
		out = output;
		for (i = count; i != 0; i--) {
			x = *in++;
			y = (design[0] * x + design[1] * x1 + design[2] * x2 + design[3] * y1 +
			     design[4] * y2) >> 12;
			y = sceSasAsSynthClamp16(y);
			*out++ = y;
			x2 = x1;
			x1 = x;
			y2 = y1;
			y1 = y;
		}
		state[0] = x1;
		state[1] = x2;
		state[2] = y1;
		state[3] = y2;
		ran = 1;
	}
	return ran;
}

/* the largest magnitude of the samples */
void sceSasAsSynthVoicePeak(SceSasAsSynthVoiceStatus *status, const SceInt16 *samples,
			    SceUInt32 count)
{
	SceInt32 magnitude;
	SceUInt16 peak = 0;

	for (; count != 0; count--) {
		magnitude = *samples++;
		if (magnitude < 0)
			magnitude = -magnitude;
		if (magnitude >= peak)
			peak = magnitude;
	}
	status->peak = peak;
}

/* input to output at a gain ramped from from to to (4.12): a copy, silence, or a scale */
void sceSasAsSynthGain(SceInt16 *output, const SceInt16 *input, SceUInt32 count, SceInt32 from,
		       SceInt32 to)
{
	SceInt32 gain, step;
	SceUInt32 i;

	if (from == to) {
		if (from == 0) {
			if (count != 0)
				memset(output, 0, count * sizeof(SceInt16));
		} else if (from == 0x1000) {
			for (i = 0; i < count; i++)
				output[i] = input[i];
		} else {
			/* Sony's product is 16 bits wide here */
			for (i = 0; i < count; i++)
				output[i] = (SceInt16)(input[i] * from) >> 12;
		}
		return;
	}
	gain = from << 16;
	step = ((to << 16) - (from << 16)) / (SceInt32)count;
	for (i = 0; i < count; i++) {
		output[i] = ((gain >> 16) * input[i]) >> 12;
		gain += step;
	}
}

/* samples into a buss at a gain ramped from from to to (4.12), replacing what it held */
__attribute__((noinline, noclone))
void sceSasAsSynthMixReplace(SceInt32 *buss, const SceInt16 *samples, SceUInt32 count,
			     SceInt32 from, SceInt32 to)
{
	SceInt32 gain, step;
	SceUInt32 i;

	if (from == to) {
		if (from == 0) {
			if (count != 0)
				memset(buss, 0, count * sizeof(SceInt32));
		} else if (from == 0x1000) {
			for (i = 0; i < count; i++)
				buss[i] = samples[i];
		} else {
			for (i = 0; i < count; i++)
				buss[i] = (samples[i] * from) >> 12;
		}
		return;
	}
	gain = from << 16;
	step = ((to << 16) - (from << 16)) / (SceInt32)count;
	for (i = 0; i < count; i++) {
		buss[i] = ((gain >> 16) * samples[i]) >> 12;
		gain += step;
	}
}

/* samples added into a buss at a gain ramped from from to to (4.12) */
void sceSasAsSynthMixAdd(SceInt32 *buss, const SceInt16 *samples, SceUInt32 count, SceInt32 from,
			 SceInt32 to)
{
	SceInt32 gain, step;
	SceUInt32 i;

	if (from == to) {
		if (from == 0)
			return;
		if (from == 0x1000) {
			for (i = 0; i < count; i++)
				buss[i] += samples[i];
		} else {
			for (i = 0; i < count; i++)
				buss[i] += (samples[i] * from) >> 12;
		}
		return;
	}
	gain = from << 16;
	step = ((to << 16) - (from << 16)) / (SceInt32)count;
	for (i = 0; i < count; i++) {
		buss[i] += ((gain >> 16) * samples[i]) >> 12;
		gain += step;
	}
}
