/* -----------------------------------------------------------------------
 *  <copyright file="CmykBlackConverter.h" company="Global Graphics Software Ltd">
 *      Copyright (c) 2023 Global Graphics Software Ltd. All rights reserved.
 *  </copyright>
 *  <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations regarding the use or
 *  results of use of this example.
 *  </summary>
 * -----------------------------------------------------------------------
 */

#pragma once

#include <jawsmako/jawsmako.h>
#include <jawsmako/customtransform.h>

#define OVERPRINT_MODE     1
#define OVERPRINT_FILL     2
#define OVERPRINT_STROKE   4

using namespace JawsMako;

class CCmykBlackConverterImplementation : public ICustomTransform::IImplementation
{
public:
    CCmykBlackConverterImplementation(const IJawsMakoPtr& jawsMako, bool useDeviceN, bool doNotApplyOverprint);
    IDOMNodePtr transformGlyphs(IImplementation* genericImplementation, const IDOMGlyphsPtr& glyphs, bool& changed, const CTransformState& state) override;
    IDOMNodePtr transformPath(IImplementation* genericImplementation, const IDOMPathNodePtr& path, bool& changed, const CTransformState& state) override;
    IDOMNodePtr transformCharPathGroup(IImplementation* genericImplementation, const IDOMCharPathGroupPtr& group,
                                       bool& changed, bool transformChildren, const CTransformState& state) override;


private:
    bool colorIsCmykRichBlack(const IDOMColorPtr& color) const;
    IDOMColorPtr transformColor(const IDOMColorPtr& inColor) const;     // NOLINT(clang-diagnostic-overloaded-virtual)
    IDOMBrushPtr transformBrush(const IDOMBrushPtr& inBrush) const;     // NOLINT(clang-diagnostic-overloaded-virtual)
    IDOMImagePtr transformImage(const IDOMImagePtr &inImage) const;
    IDOMImagePtr getFilteredImage(const IDOMImagePtr &image, uint8 bps) const;
    IDOMColorSpaceDeviceNPtr makeNewDeviceNColorSpace(
        const EDLSysString& spotColorName, const std::vector<float>& cmykValues) const;
    IDOMColorPtr makeNewDeviceNColor(const IDOMColorSpaceDeviceNPtr& deviceNSpace, double opacity, double inkValue) const;


    template <class T>
    bool transformStroke(const T& node);

	template <class T>
    bool transformFill(const T& node);

    IJawsMakoPtr m_jawsMako;
    bool m_useDeviceN;
    bool m_doNotApplyOverprint;
    IDOMColorSpacePtr m_flatBlackColorSpace;
    IDOMColorPtr m_flatBlack;
};
