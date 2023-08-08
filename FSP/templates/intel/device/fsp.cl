#ifndef __FSP_CL__
#define __FSP_CL__

#define CL_AUTORUN                      \
__attribute__((max_global_work_dim(0))) \
__attribute__((autorun))                \
__kernel void

#define CL_SINGLE_TASK                      \
__attribute__((uses_global_work_offset(0))) \
__attribute__((max_global_work_dim(0)))     \
__kernel void


#define SINK_MAX_EOS 60
typedef struct __attribute__((aligned(4))) {
    unsigned int received;
    bool EOS[SINK_MAX_EOS];
} mw_context_t;


typedef union {
    unsigned int i;
    float f;
} rng_state_t;

inline unsigned int next_int(rng_state_t * s)
{
    s->i = (s->i >> 1) | (((s->i >> 0) ^ (s->i >> 12) ^ (s->i >> 6) ^ (s->i >> 7)) << 31);
    return s->i;
}

// Generates a float random number [1.0, 2.0[
inline float next_float(rng_state_t * s)
{
    rng_state_t _s;
    s->i = (s->i >> 1) | (((s->i >> 0) ^ (s->i >> 12) ^ (s->i >> 6) ^ (s->i >> 7)) << 31);
    _s.i = ((s->i & 0x007fffff) | 0x3f800000);
    return _s.f;
}

#if defined(INTELFPGA_CL)
typedef uint header_t;
typedef uint header_elems_t;
#else
typedef cl_uint header_t;
typedef cl_uint header_elems_t;
#endif

inline header_t header_new(const bool close, const bool ready, const header_elems_t size)
{
    header_t v = 0;
    v = (close << 31) | (ready << 30) | (size & 0x3FFFFFFF);
    return v;
}

inline bool header_close(const header_t h)
{
    return (h >> 31);
}

inline bool header_ready(const header_t h)
{
    return (h >> 30) & 1;
}

inline header_elems_t header_size(const header_t h)
{
    return (h & 0x3FFFFFFF);
}

/* USEFUL MACROS */
#define PRIMITIVE_CAT(a, b) a ## b
#define CAT(a, b)           PRIMITIVE_CAT(a, b)

#define REP1(X)   X(0)
#define REP2(X)   REP1(X)   X(1)
#define REP3(X)   REP2(X)   X(2)
#define REP4(X)   REP3(X)   X(3)
#define REP5(X)   REP4(X)   X(4)
#define REP6(X)   REP5(X)   X(5)
#define REP7(X)   REP6(X)   X(6)
#define REP8(X)   REP7(X)   X(7)
#define REP9(X)   REP8(X)   X(8)
#define REP10(X)  REP9(X)   X(9)
#define REP11(X)  REP10(X)  X(10)
#define REP12(X)  REP11(X)  X(11)
#define REP13(X)  REP12(X)  X(12)
#define REP14(X)  REP13(X)  X(13)
#define REP15(X)  REP14(X)  X(14)
#define REP16(X)  REP15(X)  X(15)
#define REP17(X)  REP16(X)  X(16)
#define REP18(X)  REP17(X)  X(17)
#define REP19(X)  REP18(X)  X(18)
#define REP20(X)  REP19(X)  X(19)
#define REP21(X)  REP20(X)  X(20)
#define REP22(X)  REP21(X)  X(21)
#define REP23(X)  REP22(X)  X(22)
#define REP24(X)  REP23(X)  X(23)
#define REP25(X)  REP24(X)  X(24)
#define REP26(X)  REP25(X)  X(25)
#define REP27(X)  REP26(X)  X(26)
#define REP28(X)  REP27(X)  X(27)
#define REP29(X)  REP28(X)  X(28)
#define REP30(X)  REP29(X)  X(29)
#define REP31(X)  REP30(X)  X(30)
#define REP32(X)  REP31(X)  X(31)
#define REP33(X)  REP32(X)  X(32)
#define REP34(X)  REP33(X)  X(33)
#define REP35(X)  REP34(X)  X(34)
#define REP36(X)  REP35(X)  X(35)
#define REP37(X)  REP36(X)  X(36)
#define REP38(X)  REP37(X)  X(37)
#define REP39(X)  REP38(X)  X(38)
#define REP40(X)  REP39(X)  X(39)
#define REP41(X)  REP40(X)  X(40)
#define REP42(X)  REP41(X)  X(41)
#define REP43(X)  REP42(X)  X(42)
#define REP44(X)  REP43(X)  X(43)
#define REP45(X)  REP44(X)  X(44)
#define REP46(X)  REP45(X)  X(45)
#define REP47(X)  REP46(X)  X(46)
#define REP48(X)  REP47(X)  X(47)
#define REP49(X)  REP48(X)  X(48)
#define REP50(X)  REP49(X)  X(49)
#define REP51(X)  REP50(X)  X(50)
#define REP52(X)  REP51(X)  X(51)
#define REP53(X)  REP52(X)  X(52)
#define REP54(X)  REP53(X)  X(53)
#define REP55(X)  REP54(X)  X(54)
#define REP56(X)  REP55(X)  X(55)
#define REP57(X)  REP56(X)  X(56)
#define REP58(X)  REP57(X)  X(57)
#define REP59(X)  REP58(X)  X(58)
#define REP60(X)  REP59(X)  X(59)
#define REP61(X)  REP60(X)  X(60)
#define REP62(X)  REP61(X)  X(61)
#define REP63(X)  REP62(X)  X(62)
#define REP64(X)  REP63(X)  X(63)
#define REP65(X)  REP64(X)  X(64)
#define REP66(X)  REP65(X)  X(65)
#define REP67(X)  REP66(X)  X(66)
#define REP68(X)  REP67(X)  X(67)
#define REP69(X)  REP68(X)  X(68)
#define REP70(X)  REP69(X)  X(69)
#define REP71(X)  REP70(X)  X(70)
#define REP72(X)  REP71(X)  X(71)
#define REP73(X)  REP72(X)  X(72)
#define REP74(X)  REP73(X)  X(73)
#define REP75(X)  REP74(X)  X(74)
#define REP76(X)  REP75(X)  X(75)
#define REP77(X)  REP76(X)  X(76)
#define REP78(X)  REP77(X)  X(77)
#define REP79(X)  REP78(X)  X(78)
#define REP80(X)  REP79(X)  X(79)
#define REP81(X)  REP80(X)  X(80)
#define REP82(X)  REP81(X)  X(81)
#define REP83(X)  REP82(X)  X(82)
#define REP84(X)  REP83(X)  X(83)
#define REP85(X)  REP84(X)  X(84)
#define REP86(X)  REP85(X)  X(85)
#define REP87(X)  REP86(X)  X(86)
#define REP88(X)  REP87(X)  X(87)
#define REP89(X)  REP88(X)  X(88)
#define REP90(X)  REP89(X)  X(89)
#define REP91(X)  REP90(X)  X(90)
#define REP92(X)  REP91(X)  X(91)
#define REP93(X)  REP92(X)  X(92)
#define REP94(X)  REP93(X)  X(93)
#define REP95(X)  REP94(X)  X(94)
#define REP96(X)  REP95(X)  X(95)
#define REP97(X)  REP96(X)  X(96)
#define REP98(X)  REP97(X)  X(97)
#define REP99(X)  REP98(X)  X(98)
#define REP100(X) REP99(X) X(99)
#define REP101(X) REP100(X) X(100)
#define REP102(X) REP101(X) X(101)
#define REP103(X) REP102(X) X(102)
#define REP104(X) REP103(X) X(103)
#define REP105(X) REP104(X) X(104)
#define REP106(X) REP105(X) X(105)
#define REP107(X) REP106(X) X(106)
#define REP108(X) REP107(X) X(107)
#define REP109(X) REP108(X) X(108)
#define REP110(X) REP109(X) X(109)
#define REP111(X) REP110(X) X(110)
#define REP112(X) REP111(X) X(111)
#define REP113(X) REP112(X) X(112)
#define REP114(X) REP113(X) X(113)
#define REP115(X) REP114(X) X(114)
#define REP116(X) REP115(X) X(115)
#define REP117(X) REP116(X) X(116)
#define REP118(X) REP117(X) X(117)
#define REP119(X) REP118(X) X(118)
#define REP120(X) REP119(X) X(119)
#define REP121(X) REP120(X) X(120)
#define REP122(X) REP121(X) X(121)
#define REP123(X) REP122(X) X(122)
#define REP124(X) REP123(X) X(123)
#define REP125(X) REP124(X) X(124)
#define REP126(X) REP125(X) X(125)
#define REP127(X) REP126(X) X(126)
#define REP128(X) REP127(X) X(127)
#define REP129(X) REP128(X) X(128)
#define REP130(X) REP129(X) X(129)
#define REP131(X) REP130(X) X(130)
#define REP132(X) REP131(X) X(131)
#define REP133(X) REP132(X) X(132)
#define REP134(X) REP133(X) X(133)
#define REP135(X) REP134(X) X(134)
#define REP136(X) REP135(X) X(135)
#define REP137(X) REP136(X) X(136)
#define REP138(X) REP137(X) X(137)
#define REP139(X) REP138(X) X(138)
#define REP140(X) REP139(X) X(139)
#define REP141(X) REP140(X) X(140)
#define REP142(X) REP141(X) X(141)
#define REP143(X) REP142(X) X(142)
#define REP144(X) REP143(X) X(143)
#define REP145(X) REP144(X) X(144)
#define REP146(X) REP145(X) X(145)
#define REP147(X) REP146(X) X(146)
#define REP148(X) REP147(X) X(147)
#define REP149(X) REP148(X) X(148)
#define REP150(X) REP149(X) X(149)
#define REP151(X) REP150(X) X(150)
#define REP152(X) REP151(X) X(151)
#define REP153(X) REP152(X) X(152)
#define REP154(X) REP153(X) X(153)
#define REP155(X) REP154(X) X(154)
#define REP156(X) REP155(X) X(155)
#define REP157(X) REP156(X) X(156)
#define REP158(X) REP157(X) X(157)
#define REP159(X) REP158(X) X(158)
#define REP160(X) REP159(X) X(159)
#define REP161(X) REP160(X) X(160)
#define REP162(X) REP161(X) X(161)
#define REP163(X) REP162(X) X(162)
#define REP164(X) REP163(X) X(163)
#define REP165(X) REP164(X) X(164)
#define REP166(X) REP165(X) X(165)
#define REP167(X) REP166(X) X(166)
#define REP168(X) REP167(X) X(167)
#define REP169(X) REP168(X) X(168)
#define REP170(X) REP169(X) X(169)
#define REP171(X) REP170(X) X(170)
#define REP172(X) REP171(X) X(171)
#define REP173(X) REP172(X) X(172)
#define REP174(X) REP173(X) X(173)
#define REP175(X) REP174(X) X(174)
#define REP176(X) REP175(X) X(175)
#define REP177(X) REP176(X) X(176)
#define REP178(X) REP177(X) X(177)
#define REP179(X) REP178(X) X(178)
#define REP180(X) REP179(X) X(179)
#define REP181(X) REP180(X) X(180)
#define REP182(X) REP181(X) X(181)
#define REP183(X) REP182(X) X(182)
#define REP184(X) REP183(X) X(183)
#define REP185(X) REP184(X) X(184)
#define REP186(X) REP185(X) X(185)
#define REP187(X) REP186(X) X(186)
#define REP188(X) REP187(X) X(187)
#define REP189(X) REP188(X) X(188)
#define REP190(X) REP189(X) X(189)
#define REP191(X) REP190(X) X(190)
#define REP192(X) REP191(X) X(191)
#define REP193(X) REP192(X) X(192)
#define REP194(X) REP193(X) X(193)
#define REP195(X) REP194(X) X(194)
#define REP196(X) REP195(X) X(195)
#define REP197(X) REP196(X) X(196)
#define REP198(X) REP197(X) X(197)
#define REP199(X) REP198(X) X(198)
#define REP200(X) REP199(X) X(199)
#define REP201(X) REP200(X) X(200)
#define REP202(X) REP201(X) X(201)
#define REP203(X) REP202(X) X(202)
#define REP204(X) REP203(X) X(203)
#define REP205(X) REP204(X) X(204)
#define REP206(X) REP205(X) X(205)
#define REP207(X) REP206(X) X(206)
#define REP208(X) REP207(X) X(207)
#define REP209(X) REP208(X) X(208)
#define REP210(X) REP209(X) X(209)
#define REP211(X) REP210(X) X(210)
#define REP212(X) REP211(X) X(211)
#define REP213(X) REP212(X) X(212)
#define REP214(X) REP213(X) X(213)
#define REP215(X) REP214(X) X(214)
#define REP216(X) REP215(X) X(215)
#define REP217(X) REP216(X) X(216)
#define REP218(X) REP217(X) X(217)
#define REP219(X) REP218(X) X(218)
#define REP220(X) REP219(X) X(219)
#define REP221(X) REP220(X) X(220)
#define REP222(X) REP221(X) X(221)
#define REP223(X) REP222(X) X(222)
#define REP224(X) REP223(X) X(223)
#define REP225(X) REP224(X) X(224)
#define REP226(X) REP225(X) X(225)
#define REP227(X) REP226(X) X(226)
#define REP228(X) REP227(X) X(227)
#define REP229(X) REP228(X) X(228)
#define REP230(X) REP229(X) X(229)
#define REP231(X) REP230(X) X(230)
#define REP232(X) REP231(X) X(231)
#define REP233(X) REP232(X) X(232)
#define REP234(X) REP233(X) X(233)
#define REP235(X) REP234(X) X(234)
#define REP236(X) REP235(X) X(235)
#define REP237(X) REP236(X) X(236)
#define REP238(X) REP237(X) X(237)
#define REP239(X) REP238(X) X(238)
#define REP240(X) REP239(X) X(239)
#define REP241(X) REP240(X) X(240)
#define REP242(X) REP241(X) X(241)
#define REP243(X) REP242(X) X(242)
#define REP244(X) REP243(X) X(243)
#define REP245(X) REP244(X) X(244)
#define REP246(X) REP245(X) X(245)
#define REP247(X) REP246(X) X(246)
#define REP248(X) REP247(X) X(247)
#define REP249(X) REP248(X) X(248)
#define REP250(X) REP249(X) X(249)
#define REP251(X) REP250(X) X(250)
#define REP252(X) REP251(X) X(251)
#define REP253(X) REP252(X) X(252)
#define REP254(X) REP253(X) X(253)
#define REP255(X) REP254(X) X(254)

#define CASE(i) case i: STATEMENT(i) break;
#define SWITCH_CASE(a, x) \
    switch (a) { \
        CAT(REP, x)(CASE) \
    }

#endif // __FSP_CL__
