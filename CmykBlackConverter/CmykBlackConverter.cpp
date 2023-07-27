/* -----------------------------------------------------------------------
 *  <copyright file="CmykBlackConverter.cpp" company="Global Graphics Software Ltd">
 *      Copyright (c) 2023 Global Graphics Software Ltd. All rights reserved.
 *  </copyright>
 *  <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations regarding the use or
 *  results of use of this example.
 *  </summary>
 * -----------------------------------------------------------------------
 */

#include "CmykBlackConverter.h"

// A transform to convert rich black (CMYK with K=1.0 and some ink on the other channels)
// to flat black (C=0, M=0, Y=0, K=1.0).
CCmykBlackConverterImplementation::CCmykBlackConverterImplementation(const IJawsMakoPtr& jawsMako, bool useDeviceN, bool doNotApplyOverprint) :
                                                                     m_jawsMako(jawsMako), m_useDeviceN(useDeviceN), m_doNotApplyOverprint(doNotApplyOverprint)
{
    if (useDeviceN)
    {
        const auto deviceNColorSpace = makeNewDeviceNColorSpace("FlatBlack", { 0.0f, 0.0f, 0.0f, 1.0f });
        m_flatBlackColorSpace = edlobj2IDOMColorSpace(deviceNColorSpace);
        m_flatBlack = makeNewDeviceNColor(deviceNColorSpace, 1.0, 1.0);
    }
    else
    {
        m_flatBlackColorSpace = IDOMColorSpaceDeviceCMYK::create(m_jawsMako);
        m_flatBlack = IDOMColor::createSolidCmyk(m_jawsMako, 0.0f, 0.0f, 0.0f, 1.0f);
    }
}

IDOMNodePtr CCmykBlackConverterImplementation::transformGlyphs(IImplementation* genericImplementation, const IDOMGlyphsPtr& glyphs, bool& changed, const CTransformState& state)
{
    // Transform the fill, if present
    bool alteredFill = transformFill(glyphs);
    if (alteredFill)
    {
        changed = true;
    }

    // Perform generic processing in case something needs to change inside complex brushes (eg patterns)
    bool didSomething = false;
    IDOMNodePtr result = genericImplementation->transformGlyphs(NULL, glyphs, didSomething, state);
    changed |= didSomething;
    return result;
}

IDOMNodePtr CCmykBlackConverterImplementation::transformPath(IImplementation* genericImplementation, const IDOMPathNodePtr& path, bool& changed, const CTransformState& state)
{
    // Transform the fill, if present
    bool alteredFill = transformFill(path);
    bool alteredStroke = transformStroke(path);
    if (alteredFill || alteredStroke)
    {
        changed = true;
    }

    // Perform generic processing in case something needs to change inside complex brushes (eg patterns)
    bool didSomething = false;
    IDOMNodePtr result = genericImplementation->transformPath(NULL, path, didSomething, state);
    changed |= didSomething;
    return result;
}

// For charpath groups, we need to treat any fill on any stroke path as text. But we still want to go down into
// composite brushes if needed.
IDOMNodePtr CCmykBlackConverterImplementation::transformCharPathGroup(IImplementation* genericImplementation,
    const IDOMCharPathGroupPtr& group,
    bool& changed, bool transformChildren,
    const CTransformState& state)
{
    // Ok - what situation are we dealing with here?
    if (group->getCharPathType() == IDOMCharPathGroup::eCharPath_Stroke)
    {
        // We need to transform the stroke path as if it was text
        IDOMPathNodePtr path = group->getStrokePath();
        if (!path)
        {
            // Unexpected, but allow
            return group;
        }

        // Attempt to transform the stroke brush
        bool alteredStroke = transformStroke(path);
        if (alteredStroke)
        {
            changed = true;
        }

        // Regardless, we want to descend into the brush in case they are composite. We don't want to process the path
        // further as it may be incorrectly processed as vector art. So we deal in the brush only.
        // This is needed even if the above clause resulted in changes as there may have been a masked brush with
        // a tiling pattern sub-brush. If we didn't do anything above, we still need to do this in case there are
        // composite brushes containing content we need to process.
        // For our purposes also there is no need to update the state, but we do so anyway as a matter of discipline.
        IDOMBrushPtr stroke = path->getStroke();
        if (stroke)
        {
            CTransformState pathState = state.stateInsideNode(path);
            IDOMBrushPtr transformed = genericImplementation->transformBrush(nullptr, stroke, eBUStroke, pathState);
            if (transformed != stroke)
            {
                changed = true;
                path->setStroke(transformed);
            }
        }
    }
    else
    {
        // Clipping group. Here we would ordinarily recurse. However a Mako issue (to be logged) will not result in changed being
        // set to true. If this is the only thing requiring processing on the page or in a form, the change will be lost.
        // So instead explicitly process the clipped group.
        IDOMGroupPtr clippedGroup = group->getClippedGroup();
        if (clippedGroup)
        {
            bool didSomething = false;
            // Here we always expect a group result
            IDOMGroupPtr transformed = edlobj2IDOMGroup(transformNode(genericImplementation, clippedGroup, didSomething, transformChildren, state));
            if (didSomething)
            {
                if (!transformed)
                {
                    throwEDLError(JM_ERR_GENERAL, L"Expected a group to be the result of transforming a group in the pure black transform");
                }
                group->setClippedGroup(transformed);
                changed = true;
            }
        }
    }

    // Done
    return group;
}

// Template routine to process a fill
template <class T>
bool CCmykBlackConverterImplementation::transformFill(const T& node)
{
    IDOMBrushPtr oldBrush = node->getFill();
    IDOMBrushPtr newBrush = transformBrush(oldBrush);
    if (oldBrush != newBrush)
    {
        node->setFill(newBrush);

        // Set overprint 
        if (!m_doNotApplyOverprint)
        {
            const int devParams = OVERPRINT_MODE | OVERPRINT_FILL;
            node->setProperty("DeviceParams", PValue(devParams));
        }

        return true;
    }
    return false;
}

// Template routine to process a stroke. This one need not be a template
// (only applies to paths) but let's be consistent...
template <class T>
bool CCmykBlackConverterImplementation::transformStroke(const T& node)
{
    IDOMBrushPtr oldBrush = node->getStroke();
    IDOMBrushPtr newBrush = transformBrush(oldBrush);
    if (oldBrush != newBrush)
    {
        node->setStroke(newBrush);

        // Set overprint 
        if (!m_doNotApplyOverprint)
        {
            const int devParams = OVERPRINT_MODE | OVERPRINT_STROKE;
            node->setProperty("DeviceParams", PValue(devParams));
        }

        return true;
    }
    return false;
}

// Common routine to process a brush
IDOMBrushPtr CCmykBlackConverterImplementation::transformBrush(const IDOMBrushPtr& inBrush) const
{
    IDOMBrushPtr brush = inBrush;
    if (!brush)
    {
        return brush;
    }

    switch (brush->getBrushType())
    {
    case IDOMBrush::eSolidColor:
    {
        IDOMSolidColorBrushPtr solid = edlobj2IDOMSolidColorBrush(brush);
        IDOMColorPtr oldColor = solid->getColor();
        IDOMColorPtr newColor = transformColor(oldColor);
        if (oldColor != newColor)
        {
            solid = EDL::clone(solid, m_jawsMako);
            solid->setColor(newColor);
            brush = solid;
        }
    }
    break;

    case IDOMBrush::eImage:
    {
        IDOMImageBrushPtr imageBrush = edlobj2IDOMImageBrush(brush);
        IDOMImagePtr oldImage = imageBrush->getImageSource();
        IDOMImagePtr newImage = transformImage(oldImage);
        if (oldImage != newImage)
        {
            imageBrush = EDL::clone(imageBrush, m_jawsMako);
            imageBrush->setImageSource(newImage);
            brush = imageBrush;
        }
    }
    break;

    case IDOMBrush::eMasked:
    {
        // Recurse on the sub-brush
        IDOMMaskedBrushPtr masked = edlobj2IDOMMaskedBrush(brush);
        IDOMBrushPtr oldBrush = masked->getBrush();
        IDOMBrushPtr newBrush = transformBrush(oldBrush);
        if (oldBrush != newBrush)
        {
            masked = EDL::clone(masked, m_jawsMako);
            masked->setBrush(newBrush);
            brush = masked;
        }
    }
    break;

    case IDOMBrush::eTilingPattern:
    {
        IDOMTilingPatternBrushPtr tiling = edlobj2IDOMTilingPatternBrush(brush);
        if (tiling->getPaintType() == 2)
        {
            IDOMColorPtr oldColor = tiling->getPatternColor();
            IDOMColorPtr newColor = transformColor(oldColor);
            if (oldColor != newColor)
            {
                tiling = EDL::clone(tiling, m_jawsMako);
                tiling->setPatternColor(newColor);
                brush = tiling;
            }
        }
    }
    break;

    default:
        break;
    }
    return brush;
}

// Get an image frame, applying a BitScaler filter if required.
IDOMImagePtr CCmykBlackConverterImplementation::getFilteredImage(const IDOMImagePtr &inImage, uint8 bps) const
{
    IDOMImagePtr image = inImage;

    switch (bps)
    {
        case 1:
        case 2:
        case 4:
            bps = 8;
            break;

        case 12:
            bps = 16;
            break;

        case 8:
        case 16:
            return image;

        default:
            throwEDLError(JM_ERR_GENERAL, L"Unsupported BPS");
    }

    IDOMImageBitScalerFilterPtr scaler = IDOMImageBitScalerFilter::create(m_jawsMako, bps);
    return IDOMFilteredImage::create(m_jawsMako, image, scaler);
}

IDOMImagePtr CCmykBlackConverterImplementation::transformImage(const IDOMImagePtr &inImage) const
{
    IDOMImagePtr image = inImage;

    IImageFramePtr frame = image->getImageFrame(m_jawsMako);

    IDOMColorSpacePtr colorSpace = frame->getColorSpace();

    // Is the space CMYK?
    if (!edlobj2IDOMColorSpaceDeviceCMYK(colorSpace))
    {
        return image;
    }

    // If the image is not 8 or 16 bps filter it.
    IDOMImagePtr filteredImage = getFilteredImage(inImage, frame->getBPS());

    frame = filteredImage->getImageFrame(m_jawsMako);

    uint8 numChannels = colorSpace->getNumComponents();

    eImageExtraChannelType extraChannelType = frame->getExtraChannelType();
    if (extraChannelType != eIECNone)
    {
        // It may have an extra channel.
        numChannels += 1;
    }

    if (numChannels < 4)
    {
        // Shouldn't happen.
        return image;
    }
    
    // Get the new BPS.
    uint8 bps = frame->getBPS();

    uint32 width = frame->getWidth();
    uint32 height = frame->getHeight();

    CEDLSimpleBuffer scanline;
    scanline.resize(frame->getRawBytesPerRow());

    // First see if we need to convert.
    bool richBlack = false;

    if (bps != 8)
    {
        if (bps != 16)
        {
            throwEDLError(JM_ERR_GENERAL, L"Unexpected BPS");
        }

        // Do we need to handle this?
        for (uint32 y = 0; y < height; y++)
        {
            frame->readScanLine(&scanline[0], scanline.size());

            uint16 *ptr = (uint16 *) &scanline[0];

            for (uint32 x = 0; x < scanline.size() / 2; x += numChannels)
            {
                if (ptr[x + 3] != 0xffff)
                {
                    continue;
                }

                if (ptr[x] != 0 || ptr[x + 1] != 0 || ptr[x + 2] != 0)
                {
                    richBlack = true;
                    break;
                }
            }
        }
    }
    else
    {
        for (uint32 y = 0; y < height; y++)
        {
            frame->readScanLine(&scanline[0], scanline.size());

            for (uint32 x = 0; x < scanline.size(); x += numChannels)
            {
                if (scanline[x + 3] != 0xff)
                {
                    continue;
                }

                if (scanline[x] != 0 || scanline[x + 1] != 0 || scanline[x + 2] != 0)
                {
                    richBlack = true;
                    break;
                }
            }
        }
    }

    if (!richBlack)
    {
        // Nothing to do.
        return image;
    }

    // We need to convert, Get the frame again.
    frame = filteredImage->getImageFrame(m_jawsMako);

    // Create a writer and image.
    IImageFrameWriterPtr frameWriter;
    image = IDOMRawImage::createWriterAndImage(m_jawsMako, frameWriter, m_flatBlackColorSpace, width, height, bps,
                                               frame->getXResolution(), frame->getYResolution(), extraChannelType);

    // Convert rich black to flat black.
    if (m_useDeviceN)
    {
        if (bps != 8)
        {
            CEDLSimpleBuffer outScanline;
            outScanline.resize(width * 2);

            for (uint32 y = 0; y < height; y++)
            {
                frame->readScanLine(&scanline[0], scanline.size());
                uint16* ptr = (uint16*)&scanline[0];

                memset(&outScanline[0], 0, width * 2);

                for (uint32 x = 0; x < scanline.size() / 2; x += numChannels)
                {
                    if (ptr[x + 3] == 0xffff)
                    {
                        outScanline[x / numChannels] = 0xFFFF;
                    }
                }
                frameWriter->writeScanLine(&outScanline[0]);
            }
        }
        else
        {
            CEDLSimpleBuffer outScanline;
            outScanline.resize(width);

            for (uint32 y = 0; y < height; y++)
            {
                frame->readScanLine(&scanline[0], scanline.size());

                memset(&outScanline[0], 0, width);

                for (uint32 x = 0; x < scanline.size(); x += numChannels)
                {
                    if (scanline[x + 3] == 0xff)
                    {
                        outScanline[x / numChannels] = 0xFF;
                    }
                }
                frameWriter->writeScanLine(&outScanline[0]);
            }
        }
    }
    else
    {
        if (bps != 8)
        {
            for (uint32 y = 0; y < height; y++)
            {
                frame->readScanLine(&scanline[0], scanline.size());

                uint16* ptr = (uint16*)&scanline[0];

                for (uint32 x = 0; x < scanline.size() / 2; x += numChannels)
                {
                    if (ptr[x + 3] == 0xffff)
                    {
                        ptr[x] = ptr[x + 1] = ptr[x + 2] = 0;
                    }
                }
                frameWriter->writeScanLine(&scanline[0]);
            }
        }
        else
        {
            for (uint32 y = 0; y < height; y++)
            {
                frame->readScanLine(&scanline[0], scanline.size());

                for (uint32 x = 0; x < scanline.size(); x += numChannels)
                {
                    if (scanline[x + 3] == 0xff)
                    {
                        scanline[x] = scanline[x + 1] = scanline[x + 2] = 0;
                    }
                }
                frameWriter->writeScanLine(&scanline[0]);
            }
        }
    }

    frameWriter->flushData();

    return image;
}

IDOMColorPtr CCmykBlackConverterImplementation::transformColor(const IDOMColorPtr& inColor) const
{
    IDOMColorPtr outColor = inColor;

    if (colorIsCmykRichBlack(inColor))
    {
        return m_flatBlack;
    }

    return outColor;
}

bool CCmykBlackConverterImplementation::colorIsCmykRichBlack(const IDOMColorPtr& color) const
{
    // Is the space CMYK?
    if (!edlobj2IDOMColorSpaceDeviceCMYK(color->getColorSpace()))
    {
        return false;
    }

    // Check the components
    if (color->getComponentValue(3) != 1.0f)
    {
        return false;
    }

    return (color->getComponentValue(0) != 0.0f) || (color->getComponentValue(1) != 0.0f) || (color->getComponentValue(2) != 0.0f);
}

// Create a new DeviceN color space with the given name, with one CMYK colorant, with the given
IDOMColorSpaceDeviceNPtr CCmykBlackConverterImplementation::makeNewDeviceNColorSpace(
    const EDLSysString& spotColorName, const std::vector<float>& cmykValues) const
{
    // Create a DeviceN colorant
    IDOMColorSpaceDeviceN::CColorantInfo colorant;
    colorant.name = spotColorName;
    colorant.components.resize(4);
    colorant.components[0] = cmykValues[0];
    colorant.components[1] = cmykValues[1];
    colorant.components[2] = cmykValues[2];
    colorant.components[3] = cmykValues[3];

    // Create a vector of one colorant
    IDOMColorSpaceDeviceN::CColorantInfoVect colorants;
    colorants.append(colorant);

    // Create the DeviceN space
    const auto deviceN = IDOMColorSpaceDeviceN::create(m_jawsMako, colorants, IDOMColorSpaceDeviceCMYK::create(m_jawsMako));
    return deviceN;
}

// Create a new DeviceN color of given opacity and ink value
IDOMColorPtr CCmykBlackConverterImplementation::makeNewDeviceNColor(const IDOMColorSpaceDeviceNPtr& deviceNSpace, double opacity, double inkValue) const
{
    IDOMColorPtr deviceNColor = IDOMColor::create(m_jawsMako, deviceNSpace, opacity, inkValue);
    return deviceNColor;
}
