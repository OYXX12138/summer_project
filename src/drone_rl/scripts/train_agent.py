#!/usr/bin/env python3
import os, rospy
from stable_baselines3 import PPO
from DroneNavEnv import DroneNavEnv

if __name__ == '__main__':
    env = DroneNavEnv()
    model = PPO("MlpPolicy", env, verbose=1, tensorboard_log="./tb")
    model.learn(total_timesteps=50000)
    model.save("best_model")
    print("=== 训练结束，best_model.zip 已保存 ===")