# 实时跟踪 agv 日志
journalctl -f SYSLOG_IDENTIFIER=agv_log

# 只看 error 以上（PRIORITY <= 3）
journalctl SYSLOG_IDENTIFIER=agv_log -p err

# 按模块过滤（用自定义字段）
journalctl SYSLOG_IDENTIFIER=agv_log AGV_SOURCE=planner

# 看带时间戳的完整输出
journalctl SYSLOG_IDENTIFIER=agv_log -o verbose