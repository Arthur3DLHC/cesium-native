#pragma once

#include "Library.h"
#include "RasterOverlay.h"

#include <CesiumAsync/IAssetRequest.h>
#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Projection.h>

#include <functional>
#include <memory>

namespace Cesium3DTilesSelection {

class CreditSystem;

/**
 * @brief Options for web map tile service accesses.
 */
struct UrlTemplateRasterOverlayOptions {

  /**
   * @brief A credit for the data source, which is displayed on the canvas.
   */
  std::optional<std::string> credit;

	/*
	 * @brief The url template. The {x}, {y}, {z} in this template string will be replaced by actual tile coords
	 */
	std::optional<std::string> urlTemplate;

  /*
  * @brief URL scheme zero padding for each tile coordinate.
  */
  std::map<std::string, std::string> urlSchemeZeroPadding;

	/*
	 * @brief The level bias. The {z} in url template will be replaced by the tile original level + levelBias
	 */
	std::optional<int32_t> levelBias;

  /**
   * @brief The minimum level-of-detail supported by the imagery provider.
   *
   * Take care when specifying this that the number of tiles at the minimum
   * level is small, such as four or less. A larger number is likely to
   * result in rendering problems.
   */
  std::optional<uint32_t> minimumLevel;

  /**
   * @brief The maximum level-of-detail supported by the imagery provider.
   *
   * This will be `std::nullopt` if there is no limit.
   */
  std::optional<uint32_t> maximumLevel;

  /**
   * @brief The {@link CesiumGeometry::Rectangle}, in radians, covered by the
   * image.
   */
  std::optional<CesiumGeometry::Rectangle> coverageRectangle;

  /**
   * @brief The {@link CesiumGeospatial::Projection} that is used. If not present, use {@link CesiumGeospatial::GeographicProjection} as default.
   */
  std::optional<CesiumGeospatial::Projection> projection;

  /**
   * @brief The {@link CesiumGeometry::QuadtreeTilingScheme} specifying how
   * the ellipsoidal surface is broken into tiles.
   */
  std::optional<CesiumGeometry::QuadtreeTilingScheme> tilingScheme;

  /**
   * @brief The {@link CesiumGeospatial::Ellipsoid}.
   *
   * If the `tilingScheme` is specified, this parameter is ignored and
   * the tiling scheme's ellipsoid is used instead. If neither parameter
   * is specified, the {@link CesiumGeospatial::Ellipsoid::WGS84} is used.
   */
  std::optional<CesiumGeospatial::Ellipsoid> ellipsoid;

  /**
   * @brief Pixel width of image tiles.
   */
  std::optional<uint32_t> tileWidth;

  /**
   * @brief Pixel height of image tiles.
   */
  std::optional<uint32_t> tileHeight;

  /**
   * @brief An otion to flip the y values of a tile map resource.
   */
  // std::optional<bool> flipY;

  /**
  * @bried The subdomains to use for the {s} placeholder in the URL template. 
  */
  std::vector<std::string> subdomains;
};

/**
 * @brief A {@link RasterOverlay} based on web map tile service imagery.
 */
class CESIUM3DTILESSELECTION_API UrlTemplateRasterOverlay final
    : public RasterOverlay {
public:
  /**
   * @brief Creates a new instance.
   *
   * @param name The user-given name of this overlay layer.
   * @param url The URL Template. The {x}, {y}, {z} in template string will be replaced with actual tile coords.
   * @param headers The headers. This is a list of pairs of strings of the
   * form (Key,Value) that will be inserted as request headers internally.
   * @param tmsOptions The {@link UrlTemplateTileMapRasterOverlayOptions}.
   * @param overlayOptions The {@link RasterOverlayOptions} for this instance.
   */
  UrlTemplateRasterOverlay(
      const std::string& name,
      const std::string& url,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers = {},
      const UrlTemplateRasterOverlayOptions& tmsOptions = {},
      const RasterOverlayOptions& overlayOptions = {});
  virtual ~UrlTemplateRasterOverlay() override;

  virtual CesiumAsync::Future<CreateTileProviderResult>
  createTileProvider(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
      const std::shared_ptr<CreditSystem>& pCreditSystem,
      const std::shared_ptr<IPrepareRendererResources>&
          pPrepareRendererResources,
      const std::shared_ptr<spdlog::logger>& pLogger,
      CesiumUtility::IntrusivePointer<const RasterOverlay> pOwner) const override;

private:
  std::string _url;
  std::vector<CesiumAsync::IAssetAccessor::THeader> _headers;
  UrlTemplateRasterOverlayOptions _options;
	std::string _fileExtension;
};

} // namespace Cesium3DTilesSelection
