(() => {

window._makeFakeDisplay = () => {
  const fakeDisplay = window.navigator.createVRDisplay();
  fakeDisplay.enter = async ({renderer, animate, layers}) => {
    const canvas = renderer.domElement;

    fakeDisplay.setSize(window.innerWidth * window.devicePixelRatio, window.innerHeight * window.devicePixelRatio);
    fakeDisplay.layers = layers;
    fakeDisplay.requestPresent();
    const session = await fakeDisplay.requestSession({
      exclusive: true,
    });

    renderer.vr.enabled = true;
    renderer.vr.setDevice(fakeDisplay);
    renderer.vr.setSession(session, {
      frameOfReferenceType: 'stage',
    });
    renderer.vr.setAnimationLoop(animate);

    const context = renderer.getContext();
    const [fbo, tex, depthTex, msFbo, msTex, msDepthTex] = window.browser.createRenderTarget(context, canvas.width, canvas.height, 0, 0, 0, 0);
    context.setDefaultFramebuffer(msFbo);
    canvas.framebuffer = {
      msTex,
      msDepthTex,
      tex,
      depthTex,
    };

    return session;
  };

  return fakeDisplay;
};

})();
