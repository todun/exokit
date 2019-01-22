(() => {

const localVector = new THREE.Vector3();
const localQuaternion = new THREE.Quaternion();
const localVector2 = new THREE.Vector3();
const localMatrix = new THREE.Matrix4();
const localMatrix2 = new THREE.Matrix4();
const localViewMatrix = Float32Array.from([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);

window._makeFakeDisplay = () => {
  const fakeDisplay = window.navigator.createVRDisplay();
  fakeDisplay.setSize(window.innerWidth * window.devicePixelRatio, window.innerHeight * window.devicePixelRatio);
  fakeDisplay.getEyeParameters = (getEyeParameters => function(eye) {
    if (!fakeDisplay.getStereo() && eye === 'right') {
      const result = getEyeParameters.call(this, 'right');
      result.renderWidth = 0;
      return result;
    } else {
      return getEyeParameters.apply(this, arguments);
    }
  })(fakeDisplay.getEyeParameters);
  fakeDisplay.onrequestanimationframe = fn => window.requestAnimationFrame(fn);
  fakeDisplay.onvrdisplaypresentchange = () => {
    setTimeout(() => {
      const e = new Event('vrdisplaypresentchange');
      e.display = fakeDisplay;
      window.dispatchEvent(e);
    });
  };
  fakeDisplay.onwaitgetposes = () => {
    fakeDisplay.update();

    const inputSources = fakeDisplay.gamepads;
    for (let i = 0; i < inputSources.length; i++) {
      const gamepad = fakeDisplay.gamepads[i];
      localVector.copy(fakeDisplay.position)
        .add(
          localVector2.set(-0.3 + i*0.6, -0.3, 0)
            .applyQuaternion(fakeDisplay.quaternion)
        ).toArray(gamepad.pose.position);
      fakeDisplay.quaternion.toArray(gamepad.pose.orientation);

      if (!gamepad.handedness) {
        gamepad.handedness = gamepad.hand;
      }
      if (!gamepad.pose._localPointerMatrix) {
        gamepad.pose._localPointerMatrix = new Float32Array(32);
      }
      if (!gamepad.pose.targetRay) {
        gamepad.pose.targetRay = {
          transformMatrix: new Float32Array(32),
        };
      }

      localMatrix2
        .compose(
          localVector.fromArray(gamepad.pose.position),
          localQuaternion.fromArray(gamepad.pose.orientation),
          localVector2.set(1, 1, 1)
        )
        .toArray(gamepad.pose._localPointerMatrix);
    }
  };

  fakeDisplay.update(); // initialize gamepads
  for (let i = 0; i < fakeDisplay.gamepads.length; i++) {
    fakeDisplay.gamepads[i].pose.targetRay = {
      transformMatrix: new Float32Array(16),
    };
  }

  const onends = [];
  fakeDisplay.requestSession = function({
    exclusive = true,
    stereo = false,
  } = {}) {
    const self = this;

    const session = {
      addEventListener(e, fn) {
        if (e === 'end') {
          onends.push(fn);
        }
      },
      device: self,
      baseLayer: null,
      layers,
      _frame: null, // defer
      getInputSources() {
        return this.device.gamepads;
      },
      requestFrameOfReference() {
        return Promise.resolve({});
      },
      requestAnimationFrame(fn) {
        return this.device.onrequestanimationframe(timestamp => {
          fn(timestamp, this._frame);
        });
      },
      end() {
        for (let i = 0; i < onends.length; i++) {
          onends[i]();
        }
        return self.exitPresent();
      },
      clone() {
        const o = new this.constructor();
        for (const k in this) {
          o[k] = this[k];
        }
        o._frame = o._frame.clone();
        o._frame.session = o;
        return o;
      },
    };

    const xrState = this.getState();
    const _frame = {
      session,
      views: [{
        eye: 'left',
        projectionMatrix: xrState.leftProjectionMatrix,
        _viewport: {
          x: 0,
          y: 0,
          width: xrState.renderWidth[0],
          height: xrState.renderHeight[0],
        },
      }],
      _pose: null, // defer
      getDevicePose() {
        return this._pose;
      },
      getInputPose(inputSource, coordinateSystem) {
        localMatrix.fromArray(inputSource.pose._localPointerMatrix);

        if (self.window) {
          const {xrOffset} = self.window.document;
          localMatrix
            .premultiply(
              localMatrix2.compose(
                localVector.fromArray(xrOffset.position),
                localQuaternion.fromArray(xrOffset.orientation),
                localVector2.fromArray(xrOffset.scale)
              )
              .getInverse(localMatrix2)
            );
        }

        localMatrix.toArray(inputSource.pose.targetRay.transformMatrix);

        return inputSource.pose; // XXX or _pose
      },
      clone() {
        const o = new this.constructor();
        for (const k in this) {
          o[k] = this[k];
        }
        o._pose = o._pose.clone();
        o._pose.frame = o;
        return o;
      },
    };
    session._frame = _frame;
    const _pose = {
      frame: _frame,
      getViewMatrix(view) {
        const viewMatrix = view.eye === 'left' ? xrState.leftViewMatrix : xrState.rightViewMatrix;

        if (self.window) {
          const {xrOffset} = self.window.document;

          localMatrix
            .fromArray(viewMatrix)
            .multiply(
              localMatrix2.compose(
                localVector.fromArray(xrOffset.position),
                localQuaternion.fromArray(xrOffset.orientation),
                localVector2.fromArray(xrOffset.scale)
              )
            )
            .toArray(localViewMatrix);
        } else {
          localViewMatrix.set(viewMatrix);
        }
        return localViewMatrix;
      },
      clone() {
        const o = new this.constructor();
        for (const k in this) {
          o[k] = this[k];
        }
        return o;
      },
    };
    _frame._pose = _pose;

    return Promise.resolve(session);
  };
  /* self.update(); // initialize gamepads
  for (let i = 0; i < self.gamepads.length; i++) {
    self.gamepads[i].pose.pointerMatrix = new Float32Array(16);
  } */
  fakeDisplay.enter = async ({renderer, animate, layers, stereo = false}) => {
    const canvas = renderer.domElement;

    fakeDisplay.requestPresent();
    const session = await fakeDisplay.requestSession({
      exclusive: true,
      stereo,
    });
    fakeDisplay.onlayers(layers);

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

    fakeDisplay.stereo = stereo;

    return session;
  };

  return fakeDisplay;
};

})();
