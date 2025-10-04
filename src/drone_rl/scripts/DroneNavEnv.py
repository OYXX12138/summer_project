#!/usr/bin/env python3
import gymnasium as gym, rospy, numpy as np
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
from mavros_msgs.srv import CommandBool, SetMode

class DroneNavEnv(gym.Env):
    metadata = {"render_modes": ["human"]}

    def __init__(self):
        super().__init__()
        rospy.init_node('drone_rl_env', anonymous=True)
        self.rate       = rospy.Rate(20)
        self.target     = np.array([5.0, 5.0, 3.0])
        self.pos        = np.zeros(3)
        self.sp         = PoseStamped()
        self.sp.pose.position.z = 3.0   # 初始高度
        self.sub_pos    = rospy.Subscriber('/mavros/global_position/local', Odometry, self.cb_pos)
        self.pub_pos    = rospy.Publisher('/mavros/setpoint_position/local', PoseStamped, queue_size=1)
        self.arm_srv    = rospy.ServiceProxy('/mavros/cmd/arming', CommandBool)
        self.mode_srv   = rospy.ServiceProxy('/mavros/set_mode', SetMode)
        self.observation_space = gym.spaces.Box(-np.inf, np.inf, (6,), dtype=np.float32)
        # 大动作范围 → 每步 0.05 s 可移动 1 m
        self.action_space = gym.spaces.Box(np.array([-2, -2, -1, -1]),
                                           np.array([2, 2, 1, 1]), dtype=np.float32)
        self.timer      = rospy.Timer(rospy.Duration(0.05), self._timer_cb)
        self.start_time = 0
        self.step_cnt   = 0

    def cb_pos(self, msg):
        self.pos[0] = msg.pose.pose.position.x
        self.pos[1] = msg.pose.pose.position.y
        self.pos[2] = msg.pose.pose.position.z

    def _timer_cb(self, event):
        self.sp.header.stamp = rospy.Time.now()
        self.pub_pos.publish(self.sp)

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        self.pos[:] = 0
        self.step_cnt = 0
        # 5 秒送「目标高度 3 m」
        self.sp.pose.position.x = 0.0
        self.sp.pose.position.y = 0.0
        self.sp.pose.position.z = 3.0
        for _ in range(100):
            self.sp.header.stamp = rospy.Time.now()
            self.pub_pos.publish(self.sp)
            self.rate.sleep()
        self.mode_srv(custom_mode='OFFBOARD')
        self.arm_srv(True)
        self.start_time = rospy.get_time()
        return self._get_obs(), {}

    def _get_obs(self):
        rel = self.target - self.pos
        return np.concatenate([self.pos, rel]).astype(np.float32)

    def step(self, act):
        self.step_cnt += 1
        dt = 0.05
        # 大积分步长 → 动作 1 ≈ 1 m/s
        self.sp.pose.position.x += act[0] * dt
        self.sp.pose.position.y += act[1] * dt
        self.sp.pose.position.z += act[2] * dt
        self.sp.pose.position.z = max(0.3, self.sp.pose.position.z)  # 最低高度
        obs = self._get_obs()
        dist = np.linalg.norm(obs[3:])
        reward = -dist
        if dist < 0.5:
            reward += 100.0
        done = dist < 0.5 or self.step_cnt > 600  # 30 s 上限
        return obs, reward, done, False, {}

    def render(self): pass
