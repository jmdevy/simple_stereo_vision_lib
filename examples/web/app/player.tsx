import React, { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import { useKeyboardControls } from '@react-three/drei';
import { PerspectiveCamera} from '@react-three/drei'
import { RigidBody, CapsuleCollider } from '@react-three/rapier';

const Player = () => {
  const bodyRef = useRef();
  const controls = useKeyboardControls();

  useFrame((state) => {
    const { forward, backward, left, right } = controls;

    // Apply forces based on keyboard input
    if (forward) bodyRef.current.applyImpulse({ x: 0, y: 0, z: -0.1 });
    if (backward) bodyRef.current.applyImpulse({ x: 0, y: 0, z: 0.1 });
    if (left) bodyRef.current.applyImpulse({ x: -0.1, y: 0, z: 0 });
    if (right) bodyRef.current.applyImpulse({ x: 0.1, y: 0, z: 0 });

    // Update camera position
    state.camera.position.copy(bodyRef.current.translation());
  });

  return (
    <RigidBody ref={bodyRef} colliders={false}>
      <CapsuleCollider args={[0.5, 1.5]} />
      <mesh>
        <capsuleGeometry args={[0.5, 1.5]} />
        <meshStandardMaterial color="blue" />
      </mesh>
    </RigidBody>
  );
};


export default Player;