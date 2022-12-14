
#include "Core/PrecompiledHeader.h"
#include "GSRendererHW.h"

const float GSRendererHW::SSR_UV_TOLERANCE = 1e-3f;

GSRendererHW::GSRendererHW(GSTextureCache *tc)
    : m_width(default_rt_size.x), m_height(default_rt_size.y), m_custom_width(1024), m_custom_height(1024),
      m_reset(false), m_userhacks_ts_half_bottom(-1), m_tc(tc), m_src(nullptr), m_userhacks_tcoffset(false),
      m_userhacks_tcoffset_x(0), m_userhacks_tcoffset_y(0), m_channel_shuffle(false), m_lod(GSVector2i(0, 0))
{
    m_mipmap                   = theApp.GetConfigI("mipmap_hw");
    m_upscale_multiplier       = std::max(0, theApp.GetConfigI("upscale_multiplier"));
    m_conservative_framebuffer = theApp.GetConfigB("conservative_framebuffer");
    m_accurate_date            = theApp.GetConfigB("accurate_date");

    if (theApp.GetConfigB("UserHacks")) {
        m_userhacks_enabled_gs_mem_clear = !theApp.GetConfigB("UserHacks_Disable_Safe_Features");
        m_userHacks_enabled_unscale_ptln = !theApp.GetConfigB("UserHacks_Disable_Safe_Features");
        m_userhacks_align_sprite_X       = theApp.GetConfigB("UserHacks_align_sprite_X");
        m_userHacks_merge_sprite         = theApp.GetConfigB("UserHacks_merge_pp_sprite");
        m_userhacks_ts_half_bottom       = theApp.GetConfigI("UserHacks_Half_Bottom_Override");
        m_userhacks_round_sprite_offset  = theApp.GetConfigI("UserHacks_round_sprite_offset");
        m_userHacks_HPO                  = theApp.GetConfigI("UserHacks_HalfPixelOffset");
        m_userhacks_tcoffset_x           = theApp.GetConfigI("UserHacks_TCOffsetX") / -1000.0f;
        m_userhacks_tcoffset_y           = theApp.GetConfigI("UserHacks_TCOffsetY") / -1000.0f;
        m_userhacks_tcoffset             = m_userhacks_tcoffset_x < 0.0f || m_userhacks_tcoffset_y < 0.0f;
    } else {
        m_userhacks_enabled_gs_mem_clear = true;
        m_userHacks_enabled_unscale_ptln = true;
        m_userhacks_align_sprite_X       = false;
        m_userHacks_merge_sprite         = false;
        m_userhacks_ts_half_bottom       = -1;
        m_userhacks_round_sprite_offset  = 0;
        m_userHacks_HPO                  = 0;
    }

    if (!m_upscale_multiplier) {
        m_custom_width = m_width = theApp.GetConfigI("resx");
        m_custom_height = m_height = theApp.GetConfigI("resy");
    }

    if (m_upscale_multiplier == 1) {
        m_userhacks_round_sprite_offset = 0;
        m_userhacks_align_sprite_X      = false;
        m_userHacks_merge_sprite        = false;
    }

    m_dump_root = root_hw;
}

void GSRendererHW::SetScaling()
{
    if (!m_upscale_multiplier) {
        CustomResolutionScaling();
        return;
    }

    const GSVector2i crtc_size(GetDisplayRect().width(), GetDisplayRect().height());


    const int fb_width  = std::max({(int)m_context->FRAME.FBW * 64, crtc_size.x, 512});
    int       fb_height = 1280;
    if (m_conservative_framebuffer) {
        fb_height = fb_width < 1024 ? std::max(512, crtc_size.y) : 1024;
    }

    const int  upscaled_fb_w = fb_width * m_upscale_multiplier;
    const int  upscaled_fb_h = fb_height * m_upscale_multiplier;
    const bool good_rt_size  = m_width >= upscaled_fb_w && m_height >= upscaled_fb_h;

    if (m_upscale_multiplier <= 1 || good_rt_size)
        return;

    m_tc->RemovePartial();
    m_width  = upscaled_fb_w;
    m_height = upscaled_fb_h;
    printf("Frame buffer size set to  %dx%d (%dx%d)\n", fb_width, fb_height, m_width, m_height);
}

void GSRendererHW::CustomResolutionScaling()
{
    const int crtc_width  = GetDisplayRect().width();
    const int crtc_height = GetDisplayRect().height();
    GSVector2 scaling_ratio;
    scaling_ratio.x = std::ceil(static_cast<float>(m_custom_width) / crtc_width);
    scaling_ratio.y = std::ceil(static_cast<float>(m_custom_height) / crtc_height);

    const int scissor_width  = std::min(640, static_cast<int>(m_context->SCISSOR.SCAX1 - m_context->SCISSOR.SCAX0) + 1);
    const int scissor_height = std::min(640, static_cast<int>(m_context->SCISSOR.SCAY1 - m_context->SCISSOR.SCAY0) + 1);

    GSVector2i scissored_buffer_size;
    scissored_buffer_size.x = std::max(crtc_width, scissor_width);
    scissored_buffer_size.y = std::max(crtc_height, scissor_height);

    const int framebuffer_height = static_cast<int>(std::round(scissored_buffer_size.y * scaling_ratio.y));

    if (m_width >= m_custom_width && m_height >= framebuffer_height)
        return;

    m_tc->RemovePartial();
    m_width  = std::max(m_width, default_rt_size.x);
    m_height = std::max(framebuffer_height, default_rt_size.y);
    printf("Frame buffer size set to  %dx%d (%dx%d)\n", scissored_buffer_size.x, scissored_buffer_size.y, m_width,
           m_height);
}

GSRendererHW::~GSRendererHW()
{
    delete m_tc;
}

void GSRendererHW::SetGameCRC(uint32 crc, int options)
{
    GSRenderer::SetGameCRC(crc, options);

    m_hacks.SetGameCRC(m_game);

    if (theApp.GetConfigT<HWMipmapLevel>("mipmap_hw") == HWMipmapLevel::Automatic) {
        switch (CRC::Lookup(crc).title) {
            case CRC::AceCombatZero:
            case CRC::AceCombat4:
            case CRC::AceCombat5:
            case CRC::ApeEscape2:
            case CRC::Barnyard:
            case CRC::BrianLaraInternationalCricket:
            case CRC::DarkCloud:
            case CRC::DestroyAllHumans:
            case CRC::DestroyAllHumans2:
            case CRC::FIFA03:
            case CRC::FIFA04:
            case CRC::FIFA05:
            case CRC::HarryPotterATCOS:
            case CRC::HarryPotterATGOF:
            case CRC::HarryPotterATHBP:
            case CRC::HarryPotterATPOA:
            case CRC::HarryPotterOOTP:
            case CRC::ICO:
            case CRC::Jak1:
            case CRC::Jak3:
            case CRC::LegacyOfKainDefiance:
            case CRC::NicktoonsUnite:
            case CRC::Persona3:
            case CRC::ProjectSnowblind:
            case CRC::Quake3Revolution:
            case CRC::RatchetAndClank:
            case CRC::RatchetAndClank2:
            case CRC::RatchetAndClank3:
            case CRC::RatchetAndClank4:
            case CRC::RatchetAndClank5:
            case CRC::RickyPontingInternationalCricket:
            case CRC::Shox:
            case CRC::SlamTennis:
            case CRC::SoTC:
            case CRC::SoulReaver2:
            case CRC::TheIncredibleHulkUD:
            case CRC::TombRaiderAnniversary:
            case CRC::TribesAerialAssault:
            case CRC::Whiplash:
                m_mipmap = static_cast<int>(HWMipmapLevel::Basic);
                break;
            default:
                m_mipmap = static_cast<int>(HWMipmapLevel::Off);
                break;
        }
    }
}

bool GSRendererHW::CanUpscale()
{
    if (m_hacks.m_cu && !(this->*m_hacks.m_cu)()) {
        return false;
    }

    return m_upscale_multiplier != 1 && m_regs->PMODE.EN != 0;
}

int GSRendererHW::GetUpscaleMultiplier()
{
    return m_upscale_multiplier;
}

GSVector2i GSRendererHW::GetCustomResolution()
{
    return GSVector2i(m_custom_width, m_custom_height);
}

void GSRendererHW::Reset()
{

    m_reset = true;

    GSRenderer::Reset();
}

void GSRendererHW::VSync(int field)
{
    SetScaling();

    if (m_reset) {
        m_tc->RemoveAll();

        m_reset = false;
    }

    GSRenderer::VSync(field);

    m_tc->IncAge();

    m_tc->PrintMemoryUsage();
    m_dev->PrintMemoryUsage();

    m_skip        = 0;
    m_skip_offset = 0;
}

void GSRendererHW::ResetDevice()
{
    m_tc->RemoveAll();

    GSRenderer::ResetDevice();
}

GSTexture *GSRendererHW::GetOutput(int i, int &y_offset)
{
    const GSRegDISPFB &DISPFB = m_regs->DISP[i].DISPFB;

    GIFRegTEX0 TEX0;

    TEX0.TBP0 = DISPFB.Block();
    TEX0.TBW  = DISPFB.FBW;
    TEX0.PSM  = DISPFB.PSM;


    GSTexture *t = NULL;

    if (GSTextureCache::Target *rt = m_tc->LookupTarget(TEX0, m_width, m_height, GetFramebufferHeight())) {
        t = rt->m_texture;

        const int delta = TEX0.TBP0 - rt->m_TEX0.TBP0;
        if (delta > 0 && DISPFB.FBW != 0) {
            const int pages   = delta >> 5u;
            int       y_pages = pages / DISPFB.FBW;
            y_offset          = y_pages * GSLocalMemory::m_psm[DISPFB.PSM].pgs.y;
            GL_CACHE("Frame y offset %d pixels, unit %d", y_offset, i);
        }

#ifdef ENABLE_OGL_DEBUG
        if (s_dump) {
            if (s_savef && s_n >= s_saven) {
                t->Save(m_dump_root + format("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, m_perfmon.GetFrame(), i,
                                             (int)TEX0.TBP0, psm_str(TEX0.PSM)));
            }
        }
#endif
    }

    return t;
}

GSTexture *GSRendererHW::GetFeedbackOutput()
{
    GIFRegTEX0 TEX0;

    TEX0.TBP0 = m_regs->EXTBUF.EXBP;
    TEX0.TBW  = m_regs->EXTBUF.EXBW;
    TEX0.PSM  = m_regs->DISP[m_regs->EXTBUF.FBIN & 1].DISPFB.PSM;

    GSTextureCache::Target *rt = m_tc->LookupTarget(TEX0, m_width, m_height, 0);

    GSTexture *t = rt->m_texture;

#ifdef ENABLE_OGL_DEBUG
    if (s_dump && s_savef && s_n >= s_saven)
        t->Save(m_dump_root +
                format("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, m_perfmon.GetFrame(), 3, (int)TEX0.TBP0, psm_str(TEX0.PSM)));
#endif

    return t;
}

void GSRendererHW::Lines2Sprites()
{
    ASSERT(m_vt.m_primclass == GS_SPRITE_CLASS);


    while (m_vertex.tail * 2 > m_vertex.maxcount) {
        GrowVertexBuffer();
    }


    if (m_vertex.next >= 2) {
        size_t count = m_vertex.next;

        int              i     = (int)count * 2 - 4;
        GSVertex        *s     = &m_vertex.buff[count - 2];
        GSVertex        *q     = &m_vertex.buff[count * 2 - 4];
        uint32 *RESTRICT index = &m_index.buff[count * 3 - 6];

        for (; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6) {
            GSVertex v0 = s[0];
            GSVertex v1 = s[1];

            v0.RGBAQ = v1.RGBAQ;
            v0.XYZ.Z = v1.XYZ.Z;
            v0.FOG   = v1.FOG;

            if (PRIM->TME && !PRIM->FST) {
                GSVector4 st0 = GSVector4::loadl(&v0.ST.u64);
                GSVector4 st1 = GSVector4::loadl(&v1.ST.u64);
                GSVector4 Q   = GSVector4(v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q);
                GSVector4 st  = st0.upld(st1) / Q;

                GSVector4::storel(&v0.ST.u64, st);
                GSVector4::storeh(&v1.ST.u64, st);

                v0.RGBAQ.Q = 1.0f;
                v1.RGBAQ.Q = 1.0f;
            }

            q[0] = v0;
            q[3] = v1;


            const uint16 x = v0.XYZ.X;
            v0.XYZ.X       = v1.XYZ.X;
            v1.XYZ.X       = x;

            const float s = v0.ST.S;
            v0.ST.S       = v1.ST.S;
            v1.ST.S       = s;

            const uint16 u = v0.U;
            v0.U           = v1.U;
            v1.U           = u;

            q[1] = v0;
            q[2] = v1;

            index[0] = i + 0;
            index[1] = i + 1;
            index[2] = i + 2;
            index[3] = i + 1;
            index[4] = i + 2;
            index[5] = i + 3;
        }

        m_vertex.head = m_vertex.tail = m_vertex.next = count * 2;
        m_index.tail                                  = count * 3;
    }
}

void GSRendererHW::EmulateAtst(GSVector4 &FogColor_AREF, uint8 &ps_atst, const bool pass_2)
{
    static const uint32 inverted_atst[] = {ATST_ALWAYS,   ATST_NEVER, ATST_GEQUAL, ATST_GREATER,
                                           ATST_NOTEQUAL, ATST_LESS,  ATST_LEQUAL, ATST_EQUAL};

    if (!m_context->TEST.ATE)
        return;

    const int atst = pass_2 ? inverted_atst[m_context->TEST.ATST] : m_context->TEST.ATST;

    switch (atst) {
        case ATST_LESS:
            FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
            ps_atst         = 1;
            break;
        case ATST_LEQUAL:
            FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
            ps_atst         = 1;
            break;
        case ATST_GEQUAL:
            FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
            ps_atst         = 2;
            break;
        case ATST_GREATER:
            FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
            ps_atst         = 2;
            break;
        case ATST_EQUAL:
            FogColor_AREF.a = (float)m_context->TEST.AREF;
            ps_atst         = 3;
            break;
        case ATST_NOTEQUAL:
            FogColor_AREF.a = (float)m_context->TEST.AREF;
            ps_atst         = 4;
            break;
        case ATST_NEVER:
        case ATST_ALWAYS:
        default:
            ps_atst = 0;
            break;
    }
}

void GSRendererHW::ConvertSpriteTextureShuffle(bool &write_ba, bool &read_ba)
{
    const size_t          count = m_vertex.next;
    GSVertex             *v     = &m_vertex.buff[0];
    const GIFRegXYOFFSET &o     = m_context->XYOFFSET;

    const int pos = (v[0].XYZ.X - o.OFX) & 0xFF;
    write_ba      = (pos > 112 && pos < 136);

    const float tw      = (float)(1u << m_context->TEX0.TW);
    int         tex_pos = (PRIM->FST) ? v[0].U : (int)(tw * v[0].ST.S);
    tex_pos &= 0xFF;
    read_ba = (tex_pos > 112 && tex_pos < 144);

    bool half_bottom = false;
    switch (m_userhacks_ts_half_bottom) {
        case 0:
            half_bottom = false;
            break;
        case 1:
            half_bottom = true;
            break;
        case -1:
        default:
            const int height_delta = m_src->m_valid_rect.height() - m_r.height();
            half_bottom            = abs(height_delta) <= 64;
            break;
    }

    if (PRIM->FST) {
        GL_INS("First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);

        for (size_t i = 0; i < count; i += 2) {
            if (write_ba)
                v[i].XYZ.X -= 128u;
            else
                v[i + 1].XYZ.X += 128u;

            if (read_ba)
                v[i].U -= 128u;
            else
                v[i + 1].U += 128u;

            if (!half_bottom) {
                const int        tex_offset = v[i].V & 0xF;
                const GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

                GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
                tmp = GSVector4i(tmp - offset).srl32(1) + offset;

                v[i].XYZ.Y     = (uint16)tmp.x;
                v[i].V         = (uint16)tmp.y;
                v[i + 1].XYZ.Y = (uint16)tmp.z;
                v[i + 1].V     = (uint16)tmp.w;
            }
        }
    } else {
        const float offset_8pix = 8.0f / tw;
        GL_INS("First vertex is  P: %d => %d    T: %f => %f (offset %f)", v[0].XYZ.X, v[1].XYZ.X, v[0].ST.S, v[1].ST.S,
               offset_8pix);

        for (size_t i = 0; i < count; i += 2) {
            if (write_ba)
                v[i].XYZ.X -= 128u;
            else
                v[i + 1].XYZ.X += 128u;

            if (read_ba)
                v[i].ST.S -= offset_8pix;
            else
                v[i + 1].ST.S += offset_8pix;

            if (!half_bottom) {
                const GSVector4i offset(o.OFY, o.OFY);

                GSVector4i tmp(v[i].XYZ.Y, v[i + 1].XYZ.Y);
                tmp = GSVector4i(tmp - offset).srl32(1) + offset;

                v[i].XYZ.Y = (uint16)tmp.x;
                v[i].ST.T /= 2.0f;
                v[i + 1].XYZ.Y = (uint16)tmp.y;
                v[i + 1].ST.T /= 2.0f;
            }
        }
    }

    if (write_ba)
        m_vt.m_min.p.x -= 8.0f;
    else
        m_vt.m_max.p.x += 8.0f;

    if (!half_bottom) {
        const float delta_Y = m_vt.m_max.p.y - m_vt.m_min.p.y;
        m_vt.m_max.p.y -= delta_Y / 2.0f;
    }

    if (read_ba)
        m_vt.m_min.t.x -= 8.0f;
    else
        m_vt.m_max.t.x += 8.0f;

    if (!half_bottom) {
        const float delta_T = m_vt.m_max.t.y - m_vt.m_min.t.y;
        m_vt.m_max.t.y -= delta_T / 2.0f;
    }
}

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source *tex)
{
    if (m_userHacks_HPO <= 1 || GetUpscaleMultiplier() == 1)
        return GSVector4(0.0f);

    const GSVertex  *v          = &m_vertex.buff[0];
    const GSVector2 &scale      = tex->m_texture->GetScale();
    const bool       linear     = m_vt.IsRealLinear();
    const int        t_position = v[0].U;
    GSVector4        half_offset(0.0f);


    if (PRIM->FST) {

        if (m_userHacks_HPO == 3) {
            if (!linear && t_position == 8) {
                half_offset.x = 8;
                half_offset.y = 8;
            } else if (linear && t_position == 16) {
                half_offset.x = 16;
                half_offset.y = 16;
            } else if (m_vt.m_min.p.x == -0.5f) {
                half_offset.x = 8;
                half_offset.y = 8;
            }
        } else {
            if (!linear && t_position == 8) {
                half_offset.x = 8 - 8 / scale.x;
                half_offset.y = 8 - 8 / scale.y;
            } else if (linear && t_position == 16) {
                half_offset.x = 16 - 16 / scale.x;
                half_offset.y = 16 - 16 / scale.y;
            } else if (m_vt.m_min.p.x == -0.5f) {
                half_offset.x = 8;
                half_offset.y = 8;
            }
        }

        GL_INS("offset detected %f,%f t_pos %d (linear %d, scale %f)", half_offset.x, half_offset.y, t_position, linear,
               scale.x);
    } else if (m_vt.m_eq.q) {
        const float tw = (float)(1 << m_context->TEX0.TW);
        const float th = (float)(1 << m_context->TEX0.TH);
        const float q  = v[0].RGBAQ.Q;

        half_offset.x = 0.5f * q / tw;
        half_offset.y = 0.5f * q / th;

        GL_INS("ST offset detected %f,%f (linear %d, scale %f)", half_offset.x, half_offset.y, linear, scale.x);
    }

    return half_offset;
}

GSVector4i GSRendererHW::ComputeBoundingBox(const GSVector2 &rtscale, const GSVector2i &rtsize)
{
    const GSVector4 scale  = GSVector4(rtscale.x, rtscale.y);
    const GSVector4 offset = GSVector4(-1.0f, 1.0f);
    const GSVector4 box    = m_vt.m_min.p.xyxy(m_vt.m_max.p) + offset.xxyy();
    return GSVector4i(box * scale.xyxy()).rintersect(GSVector4i(0, 0, rtsize.x, rtsize.y));
}

void GSRendererHW::MergeSprite(GSTextureCache::Source *tex)
{
    if (m_userHacks_merge_sprite && tex && tex->m_target && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
        if (PRIM->FST && GSLocalMemory::m_psm[tex->m_TEX0.PSM].fmt < 2 && ((m_vt.m_eq.value & 0xCFFFF) == 0xCFFFF)) {

            const GSVertex *v         = &m_vertex.buff[0];
            bool            is_paving = true;
            const int       first_dpX = v[1].XYZ.X - v[0].XYZ.X;
            const int       first_dpU = v[1].U - v[0].U;
            for (size_t i = 0; i < m_vertex.next; i += 2) {
                const int dpX = v[i + 1].XYZ.X - v[i].XYZ.X;
                const int dpU = v[i + 1].U - v[i].U;
                if (dpX != first_dpX || dpU != first_dpU) {
                    is_paving = false;
                    break;
                }
            }

#if 0
			GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
			GSVector4 delta_t = m_vt.m_max.t - m_vt.m_min.t;
			bool is_blit = PrimitiveOverlap() == PRIM_OVERLAP_NO;
			GL_INS("PP SAMPLER: Dp %f %f Dt %f %f. Is blit %d, is paving %d, count %d", delta_p.x, delta_p.y, delta_t.x, delta_t.y, is_blit, is_paving, m_vertex.tail);
#endif

            if (is_paving) {
                GSVertex *s = &m_vertex.buff[0];

                s[0].XYZ.X = static_cast<uint16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
                s[1].XYZ.X = static_cast<uint16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);
                s[0].XYZ.Y = static_cast<uint16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
                s[1].XYZ.Y = static_cast<uint16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

                s[0].U = static_cast<uint16>(16.0f * m_vt.m_min.t.x);
                s[0].V = static_cast<uint16>(16.0f * m_vt.m_min.t.y);
                s[1].U = static_cast<uint16>(16.0f * m_vt.m_max.t.x);
                s[1].V = static_cast<uint16>(16.0f * m_vt.m_max.t.y);

                m_vertex.head = m_vertex.tail = m_vertex.next = 2;
                m_index.tail                                  = 2;
            }
        }
    }
}

void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF &BITBLTBUF, const GSVector4i &r)
{

    m_tc->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), r);
}

void GSRendererHW::InvalidateLocalMem(const GIFRegBITBLTBUF &BITBLTBUF, const GSVector4i &r, bool clut)
{

    if (clut)
        return;

    m_tc->InvalidateLocalMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r);
}

uint16 GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
    const float t = (1.0f - alpha) * t0 + alpha * t1;
    return (uint16)t & ~0xF;
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
    const int x = (X0 + 15) & ~0xF;
    return float(x - X0) / (float)L;
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
    const int x = (X1 - 1) & ~0xF;
    return float(x - X0) / (float)L;
}

void GSRendererHW::SwSpriteRender()
{
    ASSERT(PRIM->PRIM == GS_TRIANGLESTRIP || PRIM->PRIM == GS_SPRITE);
    ASSERT(!PRIM->FGE);
    ASSERT(!PRIM->AA1);
    ASSERT(!PRIM->FIX);

    ASSERT(!m_env.DTHE.DTHE);

    ASSERT(!m_context->TEST.ATE);
    ASSERT(!m_context->TEST.DATE);
    ASSERT(!m_context->DepthRead() && !m_context->DepthWrite());

    ASSERT(!m_context->TEX0.CSM);

    ASSERT(!m_env.PABE.PABE);

    ASSERT(!PRIM->TME || (PRIM->TME && m_context->TEX0.PSM == PSM_PSMCT32));
    ASSERT(m_context->FRAME.PSM == PSM_PSMCT32);

    ASSERT(PRIM->PRIM == GS_SPRITE || ((PRIM->IIP || m_vt.m_eq.rgba == 0xffff) && m_vt.m_eq.z == 0x1 &&
                                       (!PRIM->TME || PRIM->FST || m_vt.m_eq.q == 0x1)));

    const bool texture_mapping_enabled = PRIM->TME;

    GIFRegBITBLTBUF bitbltbuf = {};

    if (texture_mapping_enabled) {
        bitbltbuf.SBP  = m_context->TEX0.TBP0;
        bitbltbuf.SBW  = m_context->TEX0.TBW;
        bitbltbuf.SPSM = m_context->TEX0.PSM;
    }

    bitbltbuf.DBP  = m_context->FRAME.Block();
    bitbltbuf.DBW  = m_context->FRAME.FBW;
    bitbltbuf.DPSM = m_context->FRAME.PSM;

    ASSERT(m_r.x == 0 && m_r.y == 0);
    ASSERT(!PRIM->TME || (abs(m_vt.m_min.t.x) <= SSR_UV_TOLERANCE && abs(m_vt.m_min.t.y) <= SSR_UV_TOLERANCE));
    ASSERT(!PRIM->TME ||
           (abs(m_vt.m_max.t.x - m_r.z) <= SSR_UV_TOLERANCE && abs(m_vt.m_max.t.y - m_r.w) <= SSR_UV_TOLERANCE));
    ASSERT(!PRIM->TME || (m_vt.m_max.t.x <= (1 << m_context->TEX0.TW) && m_vt.m_max.t.y <= (1 << m_context->TEX0.TH)));

    GIFRegTRXPOS trxpos = {};

    trxpos.DSAX = 0;
    trxpos.DSAY = 0;
    trxpos.SSAX = 0;
    trxpos.SSAY = 0;

    GIFRegTRXREG trxreg = {};

    trxreg.RRW = m_r.z;
    trxreg.RRH = m_r.w;


    int       sx = trxpos.SSAX;
    int       sy = trxpos.SSAY;
    int       dx = trxpos.DSAX;
    int       dy = trxpos.DSAY;
    const int w  = trxreg.RRW;
    const int h  = trxreg.RRH;

    GL_INS("SwSpriteRender: Dest 0x%x W:%d F:%s, size(%d %d)", bitbltbuf.DBP, bitbltbuf.DBW, psm_str(bitbltbuf.DPSM), w,
           h);

    if (texture_mapping_enabled)
        InvalidateLocalMem(bitbltbuf, GSVector4i(sx, sy, sx + w, sy + h));
    InvalidateVideoMem(bitbltbuf, GSVector4i(dx, dy, dx + w, dy + h));

    GSOffset *RESTRICT spo =
        texture_mapping_enabled ? m_mem.GetOffset(bitbltbuf.SBP, bitbltbuf.SBW, bitbltbuf.SPSM) : nullptr;
    GSOffset *RESTRICT dpo = m_mem.GetOffset(bitbltbuf.DBP, bitbltbuf.DBW, bitbltbuf.DPSM);

    const int *RESTRICT scol = texture_mapping_enabled ? &spo->pixel.col[0][sx] : nullptr;
    int *RESTRICT       dcol = &dpo->pixel.col[0][dx];

    const bool alpha_blending_enabled = PRIM->ABE;

    const GSVertex  &v  = m_vertex.buff[m_index.buff[m_index.tail - 1]];
    const GSVector4i vc = GSVector4i(v.RGBAQ.R, v.RGBAQ.G, v.RGBAQ.B, v.RGBAQ.A).ps32();

    const GSVector4i a_mask = GSVector4i::xff000000().u8to16();

    const bool       fb_mask_enabled = m_context->FRAME.FBMSK != 0x0;
    const GSVector4i fb_mask         = GSVector4i(m_context->FRAME.FBMSK).u8to16();

    const uint8 tex0_tfx  = m_context->TEX0.TFX;
    const uint8 tex0_tcc  = m_context->TEX0.TCC;
    const uint8 alpha_b   = m_context->ALPHA.B;
    const uint8 alpha_c   = m_context->ALPHA.C;
    const uint8 alpha_fix = m_context->ALPHA.FIX;

    for (int y = 0; y < h; y++, ++sy, ++dy) {
        const uint32 *RESTRICT s = texture_mapping_enabled ? &m_mem.m_vm32[spo->pixel.row[sy]] : nullptr;
        uint32 *RESTRICT       d = &m_mem.m_vm32[dpo->pixel.row[dy]];

        ASSERT(w % 2 == 0);

        for (int x = 0; x < w; x += 2) {
            GSVector4i sc;
            if (texture_mapping_enabled) {
                ASSERT((scol[x] + 1) == scol[x + 1]);
                sc = GSVector4i::loadl(&s[scol[x]]).u8to16();

                ASSERT(tex0_tfx == 0 || tex0_tfx == 1);
                if (tex0_tfx == 0)
                    sc = sc.mul16l(vc).srl16(7).clamp8();

                if (tex0_tcc == 0)
                    sc = sc.blend(vc, a_mask);
            } else
                sc = vc;


            GSVector4i dc0;
            GSVector4i dc;

            if (alpha_blending_enabled || fb_mask_enabled) {
                ASSERT((dcol[x] + 1) == dcol[x + 1]);
                dc0 = GSVector4i::loadl(&d[dcol[x]]).u8to16();
            }

            if (alpha_blending_enabled) {
                ASSERT(m_context->ALPHA.A == 0);
                ASSERT(alpha_b == 1 || alpha_b == 2);
                ASSERT(m_context->ALPHA.D == 1);

                GSVector4i sc_alpha_vec;

                if (alpha_c == 2)
                    sc_alpha_vec = GSVector4i(alpha_fix).xxxx().ps32();
                else
                    sc_alpha_vec = (alpha_c == 0 ? sc : dc0).yyww().srl32(16).ps32().xxyy();

                switch (alpha_b) {
                    case 1:
                        dc = sc.sub16(dc0).mul16l(sc_alpha_vec).sra16(7).add16(dc0);
                        break;
                    default:
                        dc = sc.mul16l(sc_alpha_vec).sra16(7).add16(dc0);
                        break;
                }
            } else
                dc = sc;


            if (m_env.COLCLAMP.CLAMP)
                dc = dc.clamp8();
            else
                dc = dc.sll16(8).srl16(8);

            ASSERT(m_context->FBA.FBA == 0);
            dc = dc.blend(sc, a_mask);

            if (fb_mask_enabled)
                dc = dc.blend(dc0, fb_mask);

            dc = dc.pu16(GSVector4i::zero());
            ASSERT((dcol[x] + 1) == dcol[x + 1]);
            GSVector4i::storel(&d[dcol[x]], dc);
        }
    }
}

bool GSRendererHW::CanUseSwSpriteRender(bool allow_64x64_sprite)
{
    const bool r_0_0_64_64 = allow_64x64_sprite ? (m_r == GSVector4i(0, 0, 64, 64)).alltrue() : false;
    if (r_0_0_64_64 && !allow_64x64_sprite)
        return false;
    const bool r_0_0_16_16 = (m_r == GSVector4i(0, 0, 16, 16)).alltrue();
    if (!r_0_0_16_16 && !r_0_0_64_64)
        return false;
    if (PRIM->PRIM != GS_SPRITE && ((PRIM->IIP && m_vt.m_eq.rgba != 0xffff) ||
                                    (PRIM->TME && !PRIM->FST && m_vt.m_eq.q != 0x1) || m_vt.m_eq.z != 0x1))
        return false;
    if (m_vt.m_primclass != GS_TRIANGLE_CLASS && m_vt.m_primclass != GS_SPRITE_CLASS)
        return false;
    if (PRIM->PRIM != GS_TRIANGLESTRIP && PRIM->PRIM != GS_SPRITE)
        return false;
    if (m_vt.m_primclass == GS_TRIANGLE_CLASS && (PRIM->PRIM != GS_TRIANGLESTRIP || m_vertex.tail != 4))
        return false;
    if (m_vt.m_primclass == GS_SPRITE_CLASS && (PRIM->PRIM != GS_SPRITE || m_vertex.tail != 2))
        return false;
    if (m_context->DepthRead() || m_context->DepthWrite())
        return false;
    if (m_context->FRAME.PSM != PSM_PSMCT32)
        return false;
    if (PRIM->TME) {

        if (m_context->TEX0.PSM != PSM_PSMCT32)
            return false;
        if (IsMipMapDraw())
            return false;
        if (abs(m_vt.m_min.t.x) > SSR_UV_TOLERANCE || abs(m_vt.m_min.t.y) > SSR_UV_TOLERANCE)
            return false;
        if (abs(m_vt.m_max.t.x - m_r.z) > SSR_UV_TOLERANCE || abs(m_vt.m_max.t.y - m_r.w) > SSR_UV_TOLERANCE)
            return false;
        const int tw = 1 << m_context->TEX0.TW;
        const int th = 1 << m_context->TEX0.TH;
        if (m_vt.m_max.t.x > tw || m_vt.m_max.t.y > th)
            return false;
    }

    return true;
}

template <bool linear> void GSRendererHW::RoundSpriteOffset()
{
#if defined(DEBUG_V) || defined(DEBUG_U)
    bool debug = linear;
#endif
    const size_t count = m_vertex.next;
    GSVertex    *v     = &m_vertex.buff[0];

    for (size_t i = 0; i < count; i += 2) {

        const int    ox  = m_context->XYOFFSET.OFX;
        const int    X0  = v[i].XYZ.X - ox;
        const int    X1  = v[i + 1].XYZ.X - ox;
        const int    Lx  = (v[i + 1].XYZ.X - v[i].XYZ.X);
        const float  ax0 = alpha0(Lx, X0, X1);
        const float  ax1 = alpha1(Lx, X0, X1);
        const uint16 tx0 = Interpolate_UV(ax0, v[i].U, v[i + 1].U);
        const uint16 tx1 = Interpolate_UV(ax1, v[i].U, v[i + 1].U);
#ifdef DEBUG_U
        if (debug) {
            fprintf(stderr, "u0:%d and u1:%d\n", v[i].U, v[i + 1].U);
            fprintf(stderr, "a0:%f and a1:%f\n", ax0, ax1);
            fprintf(stderr, "t0:%d and t1:%d\n", tx0, tx1);
        }
#endif

        const int    oy  = m_context->XYOFFSET.OFY;
        const int    Y0  = v[i].XYZ.Y - oy;
        const int    Y1  = v[i + 1].XYZ.Y - oy;
        const int    Ly  = (v[i + 1].XYZ.Y - v[i].XYZ.Y);
        const float  ay0 = alpha0(Ly, Y0, Y1);
        const float  ay1 = alpha1(Ly, Y0, Y1);
        const uint16 ty0 = Interpolate_UV(ay0, v[i].V, v[i + 1].V);
        const uint16 ty1 = Interpolate_UV(ay1, v[i].V, v[i + 1].V);
#ifdef DEBUG_V
        if (debug) {
            fprintf(stderr, "v0:%d and v1:%d\n", v[i].V, v[i + 1].V);
            fprintf(stderr, "a0:%f and a1:%f\n", ay0, ay1);
            fprintf(stderr, "t0:%d and t1:%d\n", ty0, ty1);
        }
#endif

#ifdef DEBUG_U
        if (debug)
            fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
        if (debug)
            fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].V, v[i + 1].V);
#endif

#if 1
        if (linear) {
            const int Lu = v[i + 1].U - v[i].U;
            if ((Lu > 0) && (Lu <= (Lx + 32))) {
                v[i + 1].U -= 8;
            }
        } else {
            if (tx0 <= tx1) {
                v[i].U     = tx0;
                v[i + 1].U = tx1 + 16;
            } else {
                v[i].U     = tx0 + 15;
                v[i + 1].U = tx1;
            }
        }
#endif
#if 1
        if (linear) {
            const int Lv = v[i + 1].V - v[i].V;
            if ((Lv > 0) && (Lv <= (Ly + 32))) {
                v[i + 1].V -= 8;
            }
        } else {
            if (ty0 <= ty1) {
                v[i].V     = ty0;
                v[i + 1].V = ty1 + 16;
            } else {
                v[i].V     = ty0 + 15;
                v[i + 1].V = ty1;
            }
        }
#endif

#ifdef DEBUG_U
        if (debug)
            fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
        if (debug)
            fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].V, v[i + 1].V);
#endif
    }
}

void GSRendererHW::Draw()
{
    if (m_dev->IsLost() || IsBadFrame()) {
        GL_INS("Warning skipping a draw call (%d)", s_n);
        return;
    }
    GL_PUSH("HW Draw %d", s_n);

    const GSDrawingEnvironment &env     = m_env;
    GSDrawingContext           *context = m_context;
    const GSLocalMemory::psm_t &tex_psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];

    if (PRIM->TME && !IsMipMapActive())
        m_context->ComputeFixedTEX0(m_vt.m_min.t.xyxy(m_vt.m_max.t));


    const GIFRegTEST  TEST  = context->TEST;
    const GIFRegFRAME FRAME = context->FRAME;
    const GIFRegZBUF  ZBUF  = context->ZBUF;

    uint32 fm = context->FRAME.FBMSK;
    uint32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

    if (PRIM->TME && tex_psm.pal > 0)
        m_mem.m_clut.Read32(context->TEX0, env.TEXA);

    context->TEST.ATE = context->TEST.ATE && !GSRenderer::TryAlphaTest(fm, zm);

    context->FRAME.FBMSK = fm;
    context->ZBUF.ZMSK   = zm != 0;

    const bool no_rt = (context->ALPHA.IsCd() && PRIM->ABE && (context->FRAME.PSM == 1));
    const bool no_ds =
        !no_rt && ((zm != 0 && m_context->TEST.ZTST <= ZTST_ALWAYS && !m_context->TEST.DATE) ||
                   (context->FRAME.FBP == context->ZBUF.ZBP && !PRIM->TME && zm == 0 && fm == 0 && context->TEST.ZTE));

    const bool      draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);
    const GSVector4 delta_p         = m_vt.m_max.p - m_vt.m_min.p;
    const bool      single_page     = (delta_p.x <= 64.0f) && (delta_p.y <= 64.0f);

    if (m_channel_shuffle) {
        m_channel_shuffle = draw_sprite_tex && (m_context->TEX0.PSM == PSM_PSMT8) && single_page;
        if (m_channel_shuffle) {
            GL_CACHE("Channel shuffle effect detected SKIP");
            return;
        }
    } else if (draw_sprite_tex && m_context->FRAME.Block() == m_context->TEX0.TBP0) {
        if ((m_context->TEX0.PSM == PSM_PSMT8) && single_page) {
            GL_INS("Channel shuffle effect detected");
            m_channel_shuffle = true;
        } else {
            GL_DBG("Special post-processing effect not supported");
            m_channel_shuffle = false;
        }
    } else {
        m_channel_shuffle = false;
    }

    GIFRegTEX0 TEX0;

    TEX0.TBP0 = context->FRAME.Block();
    TEX0.TBW  = context->FRAME.FBW;
    TEX0.PSM  = context->FRAME.PSM;

    GSTextureCache::Target *rt     = NULL;
    GSTexture              *rt_tex = NULL;
    if (!no_rt) {
        rt     = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true, fm);
        rt_tex = rt->m_texture;
    }

    TEX0.TBP0 = context->ZBUF.Block();
    TEX0.TBW  = context->FRAME.FBW;
    TEX0.PSM  = context->ZBUF.PSM;

    GSTextureCache::Target *ds     = NULL;
    GSTexture              *ds_tex = NULL;
    if (!no_ds) {
        ds     = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, context->DepthWrite());
        ds_tex = ds->m_texture;
    }

    m_src             = nullptr;
    m_texture_shuffle = false;

    if (PRIM->TME) {
        GIFRegCLAMP MIP_CLAMP = context->CLAMP;
        m_lod                 = GSVector2i(0, 0);

        if (IsMipMapActive()) {
            const int interpolation = (context->TEX1.MMIN & 1) + 1;

            int       k   = (m_context->TEX1.K + 8) >> 4;
            int       lcm = m_context->TEX1.LCM;
            const int mxl = std::min<int>((int)m_context->TEX1.MXL, 6);

            if ((int)m_vt.m_lod.x >= mxl) {
                k   = mxl;
                lcm = 1;
            }

            if (PRIM->FST) {
                ASSERT(lcm == 1);
                ASSERT(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3);

                lcm = 1;
            }

            if (lcm == 1) {
                m_lod.x = std::max<int>(k, 0);
                m_lod.y = m_lod.x;
            } else {
                if (interpolation == 2) {
                    m_lod.x = std::max<int>((int)floor(m_vt.m_lod.x), 0);
                } else {
#if 0
					m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625), 0);
#else
                    if (ceil(m_vt.m_lod.x) < m_vt.m_lod.y)
                        m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625 + 0.01), 0);
                    else
                        m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625), 0);
#endif
                }

                m_lod.y = std::max<int>((int)ceil(m_vt.m_lod.y), 0);
            }

            m_lod.x = std::min<int>(m_lod.x, mxl);
            m_lod.y = std::min<int>(m_lod.y, mxl);

            TEX0 = GetTex0Layer(m_lod.x);

            MIP_CLAMP.MINU >>= m_lod.x;
            MIP_CLAMP.MINV >>= m_lod.x;
            MIP_CLAMP.MAXU >>= m_lod.x;
            MIP_CLAMP.MAXV >>= m_lod.x;

            for (int i = 0; i < m_lod.x; i++) {
                m_vt.m_min.t *= 0.5f;
                m_vt.m_max.t *= 0.5f;
            }

            GL_CACHE("Mipmap LOD %d %d (%f %f) new size %dx%d (K %d L %u)", m_lod.x, m_lod.y, m_vt.m_lod.x,
                     m_vt.m_lod.y, 1 << TEX0.TW, 1 << TEX0.TH, m_context->TEX1.K, m_context->TEX1.L);
        } else {
            TEX0 = GetTex0Layer(0);
        }

        m_context->offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

        GSVector4i r;

        GetTextureMinMax(r, TEX0, MIP_CLAMP, m_vt.IsLinear());

        m_src = tex_psm.depth ? m_tc->LookupDepthSource(TEX0, env.TEXA, r) : m_tc->LookupSource(TEX0, env.TEXA, r);

        if (IsMipMapActive() && m_mipmap == 2 && !tex_psm.depth) {
            const GSVector4 tmin = m_vt.m_min.t;
            const GSVector4 tmax = m_vt.m_max.t;

            for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++) {
                const GIFRegTEX0 &MIP_TEX0 = GetTex0Layer(layer);

                m_context->offset.tex = m_mem.GetOffset(MIP_TEX0.TBP0, MIP_TEX0.TBW, MIP_TEX0.PSM);

                MIP_CLAMP.MINU >>= 1;
                MIP_CLAMP.MINV >>= 1;
                MIP_CLAMP.MAXU >>= 1;
                MIP_CLAMP.MAXV >>= 1;

                m_vt.m_min.t *= 0.5f;
                m_vt.m_max.t *= 0.5f;

                GetTextureMinMax(r, MIP_TEX0, MIP_CLAMP, m_vt.IsLinear());

                m_src->UpdateLayer(MIP_TEX0, r, layer - m_lod.x);
            }

            m_vt.m_min.t = tmin;
            m_vt.m_max.t = tmax;
        }

        m_texture_shuffle = (GSLocalMemory::m_psm[context->FRAME.PSM].bpp == 16) && (tex_psm.bpp == 16) &&
                            draw_sprite_tex && m_src->m_32_bits_fmt;

        if (m_texture_shuffle && m_vertex.next < 3 && PRIM->FST && (m_context->FRAME.FBMSK == 0)) {

            const GSVertex *v = &m_vertex.buff[0];
            m_texture_shuffle = ((v[1].U - v[0].U) < 256) || m_context->FRAME.Block() == m_context->TEX0.TBP0 ||
                                ((m_context->SCISSOR.SCAX1 - m_context->SCISSOR.SCAX0) < 32);

            GL_INS("WARNING: Possible misdetection of effect, texture shuffle is %s",
                   m_texture_shuffle ? "Enabled" : "Disabled");
        }

        ASSERT(!m_texture_shuffle || (context->CLAMP.WMS < 3 && context->CLAMP.WMT < 3));

        if (m_src->m_target && m_context->TEX0.PSM == PSM_PSMT8 && single_page && draw_sprite_tex) {
            GL_INS("Channel shuffle effect detected (2nd shot)");
            m_channel_shuffle = true;
        } else {
            m_channel_shuffle = false;
        }
    }
    if (rt) {
        rt->m_32_bits_fmt = m_texture_shuffle || (GSLocalMemory::m_psm[context->FRAME.PSM].bpp != 16);
    }

    if (s_dump) {
        const uint64 frame = m_perfmon.GetFrame();

        std::string s;

        if (s_n >= s_saven) {
            s = format("%05d_context.txt", s_n);

            m_env.Dump(m_dump_root + s);
            m_context->Dump(m_dump_root + s);
        }

        if (s_savet && s_n >= s_saven && m_src) {
            s = format("%05d_f%lld_itex_%05x_%s_%d%d_%02x_%02x_%02x_%02x.dds", s_n, frame, (int)context->TEX0.TBP0,
                       psm_str(context->TEX0.PSM), (int)context->CLAMP.WMS, (int)context->CLAMP.WMT,
                       (int)context->CLAMP.MINU, (int)context->CLAMP.MAXU, (int)context->CLAMP.MINV,
                       (int)context->CLAMP.MAXV);

            m_src->m_texture->Save(m_dump_root + s);

            if (m_src->m_palette) {
                s = format("%05d_f%lld_itpx_%05x_%s.dds", s_n, frame, context->TEX0.CBP, psm_str(context->TEX0.CPSM));

                m_src->m_palette->Save(m_dump_root + s);
            }
        }

        if (s_save && s_n >= s_saven) {
            s = format("%05d_f%lld_rt0_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

            if (rt)
                rt->m_texture->Save(m_dump_root + s);
        }

        if (s_savez && s_n >= s_saven) {
            s = format("%05d_f%lld_rz0_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

            if (ds_tex)
                ds_tex->Save(m_dump_root + s);
        }
    }

    m_r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(context->scissor.in));

    if (m_hacks.m_oi && !(this->*m_hacks.m_oi)(rt_tex, ds_tex, m_src)) {
        GL_INS("Warning skipping a draw call (%d)", s_n);
        return;
    }

    if (!OI_BlitFMV(rt, m_src, m_r)) {
        GL_INS("Warning skipping a draw call (%d)", s_n);
        return;
    }

    if (m_userhacks_enabled_gs_mem_clear) {
        if ((m_vt.m_primclass == GS_SPRITE_CLASS) && !PRIM->TME && (!PRIM->ABE || m_context->ALPHA.IsOpaque()) &&
            (m_context->FRAME.FBMSK == 0) && !m_context->TEST.ATE &&
            (!m_context->TEST.ZTE || m_context->TEST.ZTST == ZTST_ALWAYS) && (m_vt.m_eq.rgba == 0xFFFF) && m_r.x == 0 &&
            m_r.y == 0) {

            OI_GsMemClear();

            OI_DoubleHalfClear(rt_tex, ds_tex);
        }
    }

    if ((m_upscale_multiplier > 1) && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
        const size_t count = m_vertex.next;
        GSVertex    *v     = &m_vertex.buff[0];

        if (m_userhacks_align_sprite_X) {
            const int  win_position       = v[1].XYZ.X - context->XYOFFSET.OFX;
            const bool unaligned_position = ((win_position & 0xF) == 8);
            const bool unaligned_texture  = ((v[1].U & 0xF) == 0) && PRIM->FST;
            const bool hole_in_vertex     = (count < 4) || (v[1].XYZ.X != v[2].XYZ.X);
            if (hole_in_vertex && unaligned_position && (unaligned_texture || !PRIM->FST)) {
                for (size_t i = 0; i < count; i += 2) {
                    v[i + 1].XYZ.X += 8;
                    if (unaligned_texture)
                        v[i + 1].U += 8;
                }
            }
        }

        if (PRIM->FST && draw_sprite_tex) {
            if ((m_userhacks_round_sprite_offset > 1) || (m_userhacks_round_sprite_offset == 1 && !m_vt.IsLinear())) {
                if (m_vt.IsLinear())
                    RoundSpriteOffset<true>();
                else
                    RoundSpriteOffset<false>();
            }
        } else {
            ;
        }
    }


    DrawPrims(rt_tex, ds_tex, m_src);


    context->TEST  = TEST;
    context->FRAME = FRAME;
    context->ZBUF  = ZBUF;


#if _DEBUG
    if (m_upscale_multiplier * m_r.z > m_width) {
        GL_INS("ERROR: RT width is too small only %d but require %d", m_width, m_upscale_multiplier * m_r.z);
    }
    if (m_upscale_multiplier * m_r.w > m_height) {
        GL_INS("ERROR: RT height is too small only %d but require %d", m_height, m_upscale_multiplier * m_r.w);
    }
#endif

    if (fm != 0xffffffff && rt) {
        rt->UpdateValidity(m_r);

        m_tc->InvalidateVideoMem(context->offset.fb, m_r, false);

        m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, context->FRAME.Block());
    }

    if (zm != 0xffffffff && ds) {
        ds->UpdateValidity(m_r);

        m_tc->InvalidateVideoMem(context->offset.zb, m_r, false);

        m_tc->InvalidateVideoMemType(GSTextureCache::RenderTarget, context->ZBUF.Block());
    }


    if (m_hacks.m_oo) {
        (this->*m_hacks.m_oo)();
    }

    if (s_dump) {
        const uint64 frame = m_perfmon.GetFrame();

        std::string s;

        if (s_save && s_n >= s_saven) {
            s = format("%05d_f%lld_rt1_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

            if (rt)
                rt->m_texture->Save(m_dump_root + s);
        }

        if (s_savez && s_n >= s_saven) {
            s = format("%05d_f%lld_rz1_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

            if (ds_tex)
                ds_tex->Save(m_dump_root + s);
        }

        if (s_savel > 0 && (s_n - s_saven) > s_savel) {
            s_dump = 0;
        }
    }

#ifdef DISABLE_HW_TEXTURE_CACHE
    if (rt)
        m_tc->Read(rt, m_r);
#endif
}


GSRendererHW::Hacks::Hacks()
    : m_oi_map(m_oi_list), m_oo_map(m_oo_list), m_cu_map(m_cu_list), m_oi(NULL), m_oo(NULL), m_cu(NULL)
{
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::BigMuthaTruckers, CRC::RegionCount, &GSRendererHW::OI_BigMuthaTruckers));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT3, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFXII, CRC::EU, &GSRendererHW::OI_FFXII));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFX, CRC::RegionCount, &GSRendererHW::OI_FFX));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::MetalSlug6, CRC::RegionCount, &GSRendererHW::OI_MetalSlug6));
    m_oi_list.push_back(
        HackEntry<OI_Ptr>(CRC::RozenMaidenGebetGarden, CRC::RegionCount, &GSRendererHW::OI_RozenMaidenGebetGarden));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SonicUnleashed, CRC::RegionCount, &GSRendererHW::OI_SonicUnleashed));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SuperManReturns, CRC::RegionCount, &GSRendererHW::OI_SuperManReturns));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::ArTonelico2, CRC::RegionCount, &GSRendererHW::OI_ArTonelico2));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak2, CRC::RegionCount, &GSRendererHW::OI_JakGames));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak3, CRC::RegionCount, &GSRendererHW::OI_JakGames));
    m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::JakX, CRC::RegionCount, &GSRendererHW::OI_JakGames));

    m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::OO_MajokkoALaMode2));

    m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::CU_MajokkoALaMode2));
    m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::TalesOfAbyss, CRC::RegionCount, &GSRendererHW::CU_TalesOfAbyss));
}

void GSRendererHW::Hacks::SetGameCRC(const CRC::Game &game)
{
    const uint32 hash = (uint32)((game.region << 24) | game.title);

    m_oi = m_oi_map[hash];
    m_oo = m_oo_map[hash];
    m_cu = m_cu_map[hash];

    if (game.flags & CRC::PointListPalette) {
        ASSERT(m_oi == NULL);

        m_oi = &GSRendererHW::OI_PointListPalette;
    }
}

void GSRendererHW::OI_DoubleHalfClear(GSTexture *rt, GSTexture *ds)
{

    if (!m_context->ZBUF.ZMSK && rt && ds) {
        const GSVertex             *v         = &m_vertex.buff[0];
        const GSLocalMemory::psm_t &frame_psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];

        if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.u32[0])
            return;


        const uint32 w_pages       = static_cast<uint32>(roundf(m_vt.m_max.p.x / frame_psm.pgs.x));
        const uint32 h_pages       = static_cast<uint32>(roundf(m_vt.m_max.p.y / frame_psm.pgs.y));
        const uint32 written_pages = w_pages * h_pages;

        uint32 base = 0, half = 0;
        if (m_context->FRAME.FBP > m_context->ZBUF.ZBP) {
            base = m_context->ZBUF.ZBP;
            half = m_context->FRAME.FBP;
        } else {
            base = m_context->FRAME.FBP;
            half = m_context->ZBUF.ZBP;
        }

        if (half <= (base + written_pages)) {
            const uint32 color       = v[1].RGBAQ.u32[0];
            const bool   clear_depth = (m_context->FRAME.FBP > m_context->ZBUF.ZBP);

            GL_INS("OI_DoubleHalfClear:%s: base %x half %x. w_pages %d h_pages %d fbw %d. Color %x",
                   clear_depth ? "depth" : "target", base << 5, half << 5, w_pages, h_pages, m_context->FRAME.FBW,
                   color);

            GSTexture       *t          = clear_depth ? ds : rt;
            const GSVector4i commitRect = ComputeBoundingBox(t->GetScale(), t->GetSize());
            t->CommitRegion(GSVector2i(commitRect.z, 2 * commitRect.w));

            if (clear_depth) {
                ASSERT(color == 0);
                m_dev->ClearDepth(t);
            } else {
                m_dev->ClearRenderTarget(t, color);
            }
        }
    }
}

void GSRendererHW::OI_GsMemClear()
{

    if ((m_vertex.next == 2) && m_vt.m_min.c.eq(GSVector4i(0))) {
        GSOffset        *off = m_context->offset.fb;
        const GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
        if (r.width() <= 128 || r.height() <= 128)
            return;

        GL_INS("OI_GsMemClear (%d,%d => %d,%d)", r.x, r.y, r.z, r.w);
        const int format = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

        if (format == 0) {
            for (int y = r.top; y < r.bottom; y++) {
                uint32 *RESTRICT d   = &m_mem.m_vm32[off->pixel.row[y]];
                int *RESTRICT    col = off->pixel.col[0];

                for (int x = r.left; x < r.right; x++) {
                    d[col[x]] = 0;
                }
            }
        } else if (format == 1) {
            for (int y = r.top; y < r.bottom; y++) {
                uint32 *RESTRICT d   = &m_mem.m_vm32[off->pixel.row[y]];
                int *RESTRICT    col = off->pixel.col[0];

                for (int x = r.left; x < r.right; x++) {
                    d[col[x]] &= 0xff000000;
                }
            }
        } else if (format == 2) {
            ;
#if 0
			for(int y = r.top; y < r.bottom; y++)
			{
				uint32* RESTRICT d = &m_mem.m_vm16[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] = 0;
				}
			}
#endif
        }
    }
}

bool GSRendererHW::OI_BlitFMV(GSTextureCache::Target *_rt, GSTextureCache::Source *tex, const GSVector4i &r_draw)
{
    if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && PRIM->TME && !PRIM->ABE &&
        tex && !tex->m_target && m_context->TEX0.TBW > 0) {
        GL_PUSH("OI_BlitFMV");

        GL_INS("OI_BlitFMV");



        const int tw = (int)(1 << m_context->TEX0.TW);
        const int th = (int)(1 << m_context->TEX0.TH);
        GSVector4 sRect;
        sRect.x = m_vt.m_min.t.x / tw;
        sRect.y = m_vt.m_min.t.y / th;
        sRect.z = m_vt.m_max.t.x / tw;
        sRect.w = m_vt.m_max.t.y / th;

        ASSERT(m_context->TEX0.TBP0 > m_context->FRAME.Block());
        const int  offset = (m_context->TEX0.TBP0 - m_context->FRAME.Block()) / m_context->TEX0.TBW;
        GSVector4i r_texture(r_draw);
        r_texture.y -= offset;
        r_texture.w -= offset;

        const GSVector4 dRect(r_texture);

        const GSVector4i r_full(0, 0, tw, th);
        if (GSTexture *rt = m_dev->CreateRenderTarget(tw, th)) {
            m_dev->CopyRect(tex->m_texture, rt, r_full);

            m_dev->StretchRect(tex->m_texture, sRect, rt, dRect);

            m_dev->CopyRect(rt, tex->m_texture, r_full);

            m_dev->Recycle(rt);
        }

        m_tc->Read(tex, r_texture);

        m_tc->InvalidateVideoMemSubTarget(_rt);

        return false;
    }

    return true;
}


bool GSRendererHW::OI_BigMuthaTruckers(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{

    const GIFRegTEX0 Texture = m_context->TEX0;

    GIFRegTEX0 Frame;
    Frame.TBW  = m_context->FRAME.FBW;
    Frame.TBP0 = m_context->FRAME.FBP;
    Frame.TBP0 = m_context->FRAME.Block();

    if (PRIM->TME && Frame.TBW == 10 && Texture.TBW == 10 && Frame.TBP0 == 0x00a00 && Texture.PSM == PSM_PSMT8H &&
        (m_r.y == 256 || m_r.y == 224)) {
        GL_INS("OI_BigMuthaTruckers half bottom offset");

        const size_t count  = m_vertex.next;
        GSVertex    *v      = &m_vertex.buff[0];
        const uint16 offset = (uint16)m_r.y * 16;

        for (size_t i = 0; i < count; i++)
            v[i].V += offset;
    }

    return true;
}

bool GSRendererHW::OI_DBZBTGames(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    if (t && t->m_from_target)
        return true;

    if (!CanUseSwSpriteRender(true))
        return true;

    SwSpriteRender();

    return false;
}

bool GSRendererHW::OI_FFXII(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    static uint32 *video = NULL;
    static size_t  lines = 0;

    if (lines == 0) {
        if (m_vt.m_primclass == GS_LINE_CLASS && (m_vertex.next == 448 * 2 || m_vertex.next == 512 * 2)) {
            lines = m_vertex.next / 2;
        }
    } else {
        if (m_vt.m_primclass == GS_POINT_CLASS) {
            if (m_vertex.next >= 16 * 512) {

                if (!video)
                    video = new uint32[512 * 512];

                const int ox = m_context->XYOFFSET.OFX - 8;
                const int oy = m_context->XYOFFSET.OFY - 8;

                const GSVertex *RESTRICT v = m_vertex.buff;

                for (int i = (int)m_vertex.next; i > 0; i--, v++) {
                    int x = (v->XYZ.X - ox) >> 4;
                    int y = (v->XYZ.Y - oy) >> 4;

                    if (x < 0 || x >= 448 || y < 0 || y >= (int)lines)
                        return false;

                    video[(y << 8) + (y << 7) + (y << 6) + x] = v->RGBAQ.u32[0];
                }

                return false;
            } else {
                lines = 0;
            }
        } else if (m_vt.m_primclass == GS_LINE_CLASS) {
            if (m_vertex.next == lines * 2) {

                m_dev->Recycle(t->m_texture);

                t->m_texture = m_dev->CreateTexture(512, 512);

                t->m_texture->Update(GSVector4i(0, 0, 448, lines), video, 448 * 4);

                m_vertex.buff[2] = m_vertex.buff[m_vertex.next - 2];
                m_vertex.buff[3] = m_vertex.buff[m_vertex.next - 1];

                m_index.buff[0] = 0;
                m_index.buff[1] = 1;
                m_index.buff[2] = 2;
                m_index.buff[3] = 1;
                m_index.buff[4] = 2;
                m_index.buff[5] = 3;

                m_vertex.head = m_vertex.tail = m_vertex.next = 4;
                m_index.tail                                  = 6;

                m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GS_TRIANGLE_CLASS);
            } else {
                lines = 0;
            }
        }
    }

    return true;
}

bool GSRendererHW::OI_FFX(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    const uint32 FBP = m_context->FRAME.Block();
    const uint32 ZBP = m_context->ZBUF.Block();
    const uint32 TBP = m_context->TEX0.TBP0;

    if ((FBP == 0x00d00 || FBP == 0x00000) && ZBP == 0x02100 && PRIM->TME && TBP == 0x01a00 &&
        m_context->TEX0.PSM == PSM_PSMCT16S) {
        GL_INS("OI_FFX ZB clear");
        if (ds)
            ds->Commit();
        m_dev->ClearDepth(ds);
    }

    return true;
}

bool GSRendererHW::OI_MetalSlug6(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{

    GSVertex *RESTRICT v = m_vertex.buff;

    for (int i = (int)m_vertex.next; i > 0; i--, v++) {
        const uint32 c = v->RGBAQ.u32[0];

        const uint32 r = (c >> 0) & 0xff;
        const uint32 g = (c >> 8) & 0xff;
        const uint32 b = (c >> 16) & 0xff;

        if (r == 0 && g != 0 && b != 0) {
            v->RGBAQ.u32[0] = (c & 0xffffff00) | ((g + b + 1) >> 1);
        }
    }

    m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, m_vt.m_primclass);

    return true;
}

bool GSRendererHW::OI_RozenMaidenGebetGarden(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    if (!PRIM->TME) {
        const uint32 FBP = m_context->FRAME.Block();
        const uint32 ZBP = m_context->ZBUF.Block();

        if (FBP == 0x008c0 && ZBP == 0x01a40) {

            GIFRegTEX0 TEX0;

            TEX0.TBP0 = ZBP;
            TEX0.TBW  = m_context->FRAME.FBW;
            TEX0.PSM  = m_context->FRAME.PSM;

            if (GSTextureCache::Target *tmp_rt =
                    m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true)) {
                GL_INS("OI_RozenMaidenGebetGarden FB clear");
                tmp_rt->m_texture->Commit();
                m_dev->ClearRenderTarget(tmp_rt->m_texture, 0);
            }

            return false;
        } else if (FBP == 0x00000 && m_context->ZBUF.Block() == 0x01180) {

            GIFRegTEX0 TEX0;

            TEX0.TBP0 = FBP;
            TEX0.TBW  = m_context->FRAME.FBW;
            TEX0.PSM  = m_context->ZBUF.PSM;

            if (GSTextureCache::Target *tmp_ds =
                    m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true)) {
                GL_INS("OI_RozenMaidenGebetGarden ZB clear");
                tmp_ds->m_texture->Commit();
                m_dev->ClearDepth(tmp_ds->m_texture);
            }

            return false;
        }
    }

    return true;
}

bool GSRendererHW::OI_SonicUnleashed(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{

    const GIFRegTEX0 Texture = m_context->TEX0;

    GIFRegTEX0 Frame;
    Frame.TBW  = m_context->FRAME.FBW;
    Frame.TBP0 = m_context->FRAME.FBP;
    Frame.TBP0 = m_context->FRAME.Block();
    Frame.PSM  = m_context->FRAME.PSM;

    if ((!PRIM->TME) || (GSLocalMemory::m_psm[Texture.PSM].bpp != 16) || (GSLocalMemory::m_psm[Frame.PSM].bpp != 16))
        return true;

    if ((Texture.TBP0 == Frame.TBP0) || (Frame.TBW != 16 && Texture.TBW != 16))
        return true;

    GL_INS("OI_SonicUnleashed replace draw by a copy");

    GSTextureCache::Target *src = m_tc->LookupTarget(Texture, m_width, m_height, GSTextureCache::RenderTarget, true);

    const GSVector2i size = rt->GetSize();

    const GSVector4 sRect(0, 0, 1, 1);
    const GSVector4 dRect(0, 0, size.x, size.y);

    m_dev->StretchRect(src->m_texture, sRect, rt, dRect, true, true, true, false);

    return false;
}

bool GSRendererHW::OI_PointListPalette(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    if (m_vt.m_primclass == GS_POINT_CLASS && !PRIM->TME) {
        const uint32 FBP = m_context->FRAME.Block();
        const uint32 FBW = m_context->FRAME.FBW;

        if (FBP >= 0x03f40 && (FBP & 0x1f) == 0) {
            if (m_vertex.next == 16) {
                GSVertex *RESTRICT v = m_vertex.buff;

                for (int i = 0; i < 16; i++, v++) {
                    uint32       c = v->RGBAQ.u32[0];
                    const uint32 a = c >> 24;

                    c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

                    v->RGBAQ.u32[0] = c;

                    m_mem.WritePixel32(i & 7, i >> 3, c, FBP, FBW);
                }

                m_mem.m_clut.Invalidate();

                return false;
            } else if (m_vertex.next == 256) {
                GSVertex *RESTRICT v = m_vertex.buff;

                for (int i = 0; i < 256; i++, v++) {
                    uint32       c = v->RGBAQ.u32[0];
                    const uint32 a = c >> 24;

                    c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

                    v->RGBAQ.u32[0] = c;

                    m_mem.WritePixel32(i & 15, i >> 4, c, FBP, FBW);
                }

                m_mem.m_clut.Invalidate();

                return false;
            } else {
                ASSERT(0);
            }
        }
    }

    return true;
}

bool GSRendererHW::OI_SuperManReturns(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    const GSDrawingContext *ctx = m_context;
#ifndef NDEBUG
    GSVertex *v = &m_vertex.buff[0];
#endif

    if (!(ctx->FRAME.FBP == ctx->ZBUF.ZBP && !PRIM->TME && !ctx->ZBUF.ZMSK && !ctx->FRAME.FBMSK &&
          m_vt.m_eq.rgba == 0xFFFF))
        return true;

    ASSERT(m_vertex.next == 2);
    ASSERT(m_vt.m_primclass == GS_SPRITE_CLASS);
    ASSERT((v->RGBAQ.A << 24 | v->RGBAQ.B << 16 | v->RGBAQ.G << 8 | v->RGBAQ.R) == (int)v->XYZ.Z);

    if (rt)
        rt->Commit();
    m_dev->ClearRenderTarget(rt, GSVector4(m_vt.m_min.c));

    m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, ctx->FRAME.Block());
    GL_INS("OI_SuperManReturns");

    return false;
}

bool GSRendererHW::OI_ArTonelico2(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{


    const GSVertex *v = &m_vertex.buff[0];

    if (m_vertex.next == 2 && !PRIM->TME && m_context->FRAME.FBW == 10 && v->XYZ.Z == 0 &&
        m_context->TEST.ZTST == ZTST_ALWAYS) {
        GL_INS("OI_ArTonelico2");
        if (ds)
            ds->Commit();
        m_dev->ClearDepth(ds);
    }

    return true;
}

bool GSRendererHW::OI_JakGames(GSTexture *rt, GSTexture *ds, GSTextureCache::Source *t)
{
    if (!CanUseSwSpriteRender(false))
        return true;

    SwSpriteRender();

    return false;
}


void GSRendererHW::OO_MajokkoALaMode2()
{

    const uint32 FBP = m_context->FRAME.Block();

    if (!PRIM->TME && FBP == 0x03f40) {
        GIFRegBITBLTBUF BITBLTBUF;

        BITBLTBUF.SBP  = FBP;
        BITBLTBUF.SBW  = 1;
        BITBLTBUF.SPSM = PSM_PSMCT32;

        InvalidateLocalMem(BITBLTBUF, GSVector4i(0, 0, 16, 16));
    }
}


bool GSRendererHW::CU_MajokkoALaMode2()
{

    const uint32 FBP = m_context->FRAME.Block();

    return FBP != 0x03f40;
}

bool GSRendererHW::CU_TalesOfAbyss()
{

    const uint32 FBP = m_context->FRAME.Block();

    return FBP != 0x036e0 && FBP != 0x03560 && FBP != 0x038e0;
}
