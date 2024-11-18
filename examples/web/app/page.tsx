"use client"

import "./globals.css"
import * as THREE from 'three'
import React, { useEffect, useRef, useState } from 'react'
import { Theme } from "react-daisyui";
import { Canvas, createPortal, useFrame, useThree } from '@react-three/fiber'
import { Stats, OrbitControls, useFBO, PerspectiveCamera, OrthographicCamera, Plane, shaderMaterial, Box } from '@react-three/drei'

// Create an object with keys as strings and mostly any type of value
interface Props{
    [key: string]: string | number | boolean | any
}


function Render(props: Props){
    const leftCameraFrameBuffer = useFBO(window.innerWidth, window.innerHeight, {format:THREE.RGBAFormat, type:THREE.UnsignedByteType});
    const rightCameraFrameBuffer = useFBO(window.innerWidth, window.innerHeight, {format:THREE.RGBAFormat, type:THREE.UnsignedByteType});

    const guiScene = new THREE.Scene();
    const guiCamera = React.useRef();

    // just to make component re-render on canvas size change
    useThree()

    // https://codesandbox.io/p/sandbox/lingering-cdn-0jqtr?file=%2Fsrc%2Findex.js%3A141%2C1
    useFrame(({ gl, camera, scene }) => {
        gl.autoClear = false;

        /** Render scene from camera A to a render target */
        gl.setRenderTarget(leftCameraFrameBuffer);
        gl.clear(true);
        gl.render(scene, props.leftCamera.current);
    
        /** Render scene from camera B to a different render target */
        gl.setRenderTarget(rightCameraFrameBuffer);
        gl.clear(true);
        gl.render(scene, props.rightCamera.current);
    
        // render main scene
        scene.overrideMaterial = null;
        gl.setRenderTarget(null);
        gl.render(scene, camera);

        // render GUI panels on top of main scene
        gl.render(guiScene, guiCamera.current);

        gl.autoClear = true;
    }, 1);

    return createPortal(
        <>
          <OrthographicCamera ref={guiCamera} near={0.0001} far={1} />
    
          <group position-x={-window.innerWidth/4} position-y={0} position-z={-0.1}>
            <Plane args={[window.innerWidth/2, window.innerHeight, 1]} position-y={0}>
              <meshBasicMaterial map={leftCameraFrameBuffer.texture} />
            </Plane>
    
            <Plane args={[window.innerWidth/2, window.innerHeight, 1]} position-x={window.innerWidth/2}>
              <meshBasicMaterial map={rightCameraFrameBuffer.texture} />
            </Plane>
          </group>
        </>,
        guiScene
      )
}


export default function Home() {
  let stereoParent = useRef();
  let leftCamera = useRef();
  let rightCamera = useRef();
  
  const baseline = 10.0;


  useEffect(() => {
    const waitForScene = async () => {
      return new Promise((resolve, reject) => {
        const cb = () => {
          if(leftCamera.current != undefined && rightCamera.current != undefined){
            resolve();
          }else{
            setTimeout(cb, 10);
          }
        }

        cb();
      });
    }

    waitForScene().then(() => {
      stereoParent.current.attach(leftCamera.current);
      stereoParent.current.attach(rightCamera.current);
      stereoParent.current.translateY(2);
      stereoParent.current.translateZ(8);
    });

    document.addEventListener("keydown", (event) => {
      if(event.code == "KeyA"){
        stereoParent.current.translateX(-0.5);
      }else if(event.code == "KeyD"){
        stereoParent.current.translateX(0.5);
      }else if(event.code == "KeyW"){
        stereoParent.current.translateZ(0.5);
      }else if(event.code == "KeyS"){
        stereoParent.current.translateZ(-0.5);
      }
    })
  }, []);

  // There are three cameras in the scene:
  // 1. Default (`Canvas` creates one)
  // 2. Left    (Created for stereo vision)
  // 3. Right   (Created for stereo vision)
  return (
      <Theme dataTheme="dim" className="absolute left-0 right-0 top-0 bottom-0 flex">
          <Canvas camera={{position: [0, 5, 20]}}>
            {/* Stereo pair */}
            <Box ref={stereoParent} visible={true} position={[0, 0, 0]} />
            <PerspectiveCamera ref={leftCamera} position={[-baseline/2, 0, 0]} />
            <PerspectiveCamera ref={rightCamera} position={[baseline/2, 0, 0]} />

            <ambientLight intensity={Math.PI / 2} />
            <spotLight position={[10, 10, 10]} angle={0.15} penumbra={1} decay={0} intensity={Math.PI} />
            <pointLight position={[-10, -10, -10]} decay={0} intensity={Math.PI} />
            <gridHelper args={[40, 40, 0xff0000, 'teal']} />
            <Box position={[0, 0, -10]} />

            <Render leftCamera={leftCamera} rightCamera={rightCamera}/>

            <OrbitControls />
          </Canvas>,
      </Theme>
  );
}
