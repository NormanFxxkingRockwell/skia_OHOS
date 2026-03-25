import XComponentContext from '../interface/XComponentContext';

@Entry
@Component
struct Index {
  private controller: XComponentController = new XComponentController();
  private xComponentContext?: XComponentContext = undefined;
  @State status: string = '等待 XComponent 初始化';

  build() {
    Column({ space: 16 }) {
      Text('Skia XComponent Demo')
        .fontSize(24)
        .fontWeight(FontWeight.Bold)

      Text(this.status)
        .fontSize(14)
        .fontColor('#5F5F5F')

      XComponent({
        id: 'skia_surface',
        type: XComponentType.SURFACE,
        controller: this.controller,
        libraryname: 'entry'
      })
        .onLoad((xComponentContext) => {
          this.xComponentContext = xComponentContext as XComponentContext;
          this.xComponentContext.registerSurface();
          this.xComponentContext.renderNow();
          this.status = 'XComponent 已加载，已请求 native GPU shaping 文本渲染';
        })
        .width('100%')
        .height('78%')
        .backgroundColor('#101820')

      Column() {
        Button('重新渲染文本')
          .width('100%')
          .onClick(() => {
            this.xComponentContext?.registerSurface();
            this.xComponentContext?.renderNow();
            this.status = '已请求 native 侧重绘 shaping 文本场景';
          })

        Text('当前路径: XComponent -> NativeWindow/EGL -> Skia GPU direct rendering -> HarfBuzz/ICU shaping')
          .fontSize(13)
          .fontColor('#7A7A7A')
      }
      .width('100%')
      .padding({ bottom: 12 })
    }
    .width('100%')
    .height('100%')
    .padding(16)
  }
}
