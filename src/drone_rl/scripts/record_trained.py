#!/usr/bin/env python3
import rospy, cv2, os
from stable_baselines3 import PPO
from DroneNavEnv import DroneNavEnv

env = DroneNavEnv()
model = PPO.load("best_model")
obs,_ = env.reset()
for _ in range(200):
    act,_ = model.predict(obs)
    obs,rew,done,_,_ = env.step(act)
    if done: break
print("=== 演示飞行结束 ===")