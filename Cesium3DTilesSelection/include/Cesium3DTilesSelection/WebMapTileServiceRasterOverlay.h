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
struct WebMapTileServiceRasterOverlayOptions {
  /**
   * @brief A credit for the data source, which is displayed on the canvas.
   */
  std::optional<std::string> credit;

  /*
   * @brief The WMTS tile layer
   */
  std::optional<std::string> layer;

  /*
   * @brief The WMTS service verion, default 1.0.0
   */
  std::optional<std::string> version;

  /*
   * @brief The WMTS tile layer style
   */
  std::optional<std::string> style;

  /*
   * @brief The tile matrix set
   */
  std::optional<std::string> tileMatrixSet;

  /*
   * @brief The tile format
   */
  std::optional<std::string> format;

  /*
   * @brief The access token name, for example, tk=abcd, the token name is 'tk'
   */
  std::optional<std::string> tokenName;

  /*
   * @brief The access token variable, for example, tk=abcd, the token value is
   * 'abcd'
   */
  std::optional<std::string> tokenValue;

  	/*
   * @brief The level bias. The {TileMatrix} in url template will be replaced by the tile original level + levelBias
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
   * @brief The {@link CesiumGeospatial::Projection} that is used.
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

  // LHC 20221118 以下两个参数废弃。因为 WMTS 有可遵循的规范

  /**
   * @brief An otion to flip the x values of a tile map resource.
   *
   * 由于 WMTS KVP 风格调用时无法使用 {reverseX} 模板参数，所以单加一个标志
   * because WMTS KVP Style call can not use {reverseX} template param, so add this flag.
   */
  // std::optional<bool> reverseX;

  /**
   * @brief An otion to flip the y values of a tile map resource.
   *
   * 由于 WMTS KVP 风格调用时无法使用 {reverseY} 模板参数，所以单加一个标志
   * because WMTS KVP Style call can not use {reverseY} template param, so add this flag.
   */
  // std::optional<bool> reverseY;


  /**
  * @brief A list of identifiers in the TileMatrix to use for WMTS requests, one per TileMatrix level.
  *
  * If empty, the level number will be used.
  */
  std::vector<std::string> tileMatrixLabels;

  /**
   * @brief Subdomains
   */
  std::vector<std::string> subdomains;
};

/**
 * @brief A {@link RasterOverlay} based on web map tile service imagery.
 */
class CESIUM3DTILESSELECTION_API WebMapTileServiceRasterOverlay final
    : public RasterOverlay {
public:
  // TODO: 参考
  // https://cesium.com/learn/cesiumjs/ref-doc/WebMapTileServiceImageryProvider.html
  // 改用 urlTemplate ？ （如果没有指定，则使用默认的 template？）

  /**
   * @brief Creates a new instance.
   *
   * @param name The user-given name of this overlay layer.
   * @param url The base URL.
   * @param headers The headers. This is a list of pairs of strings of the
   * form (Key,Value) that will be inserted as request headers internally.
   * @param tmsOptions The {@link WebMapTileServiceRasterOverlayOptions}.
   * @param overlayOptions The {@link RasterOverlayOptions} for this instance.
   */
  WebMapTileServiceRasterOverlay(
      const std::string& name,
      const std::string& url,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers = {},
      const WebMapTileServiceRasterOverlayOptions& tmsOptions = {},
      const RasterOverlayOptions& overlayOptions = {});
  virtual ~WebMapTileServiceRasterOverlay() override;

  virtual CesiumAsync::Future<CreateTileProviderResult> createTileProvider(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
      const std::shared_ptr<CreditSystem>& pCreditSystem,
      const std::shared_ptr<IPrepareRendererResources>&
          pPrepareRendererResources,
      const std::shared_ptr<spdlog::logger>& pLogger,
      CesiumUtility::IntrusivePointer<const RasterOverlay> pOwner)
      const override;

private:
  std::string _url;
  std::vector<CesiumAsync::IAssetAccessor::THeader> _headers;
  WebMapTileServiceRasterOverlayOptions _options;
};

} // namespace Cesium3DTilesSelection
