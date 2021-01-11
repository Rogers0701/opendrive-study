#include "EndConditionSampler.h"
#include "planning_gflags.h"

namespace PlannerHNS
{
    using State = std::array<double, 3>;
    using Condition = std::pair<State, double>;
    // double FLAGS_trajectory_time_length
    EndConditionSampler::EndConditionSampler(
        const std::array<double, 3> &init_s, const std::array<double, 3> &init_d,
        std::shared_ptr<PathTimeGraph> ptr_path_time_graph,
        std::shared_ptr<PredictionQuerier> ptr_prediction_querier)
        : init_s_(init_s), init_d_(init_d),
          feasible_region_(init_s),
          ptr_path_time_graph_(std::move(ptr_path_time_graph)),
          ptr_prediction_querier_(std::move(ptr_prediction_querier)) {}

    std::vector<Condition> EndConditionSampler::SampleLatEndConditions() const
    {
        std::vector<Condition> end_d_conditions;
        auto lane_width =
            ptr_path_time_graph_.get()->ptg_reference_info()->reference_line().GetLaneWidth(FLAGS_look_ahead_distance);
        double seed_l =
            (lane_width - car_params.width) * 0.5 - FLAGS_ego_to_lane_boundary;

        double lane_center_l = 0.0;
        double lane_left_l = 0.0;
        double lane_right_l = 0.0;
        if (ReferenceLineInfo::lane_change_state == CHANGE_DISABLE)
        {
            lane_center_l = 0.0;
            lane_left_l = seed_l;
            lane_right_l = -seed_l;
            if (!(ptr_path_time_graph_.get()
                      ->ptg_reference_info()
                      ->reference_line()
                      .current_reference_pose()
                      .enable_road_occupation_left))
            {
                std::array<double, 4> end_s_candidates = {5.0, 12.0, 27.0, 45.0};
                std::array<double, 3> end_d_candidates = {lane_center_l, lane_left_l, lane_right_l};
                for (const auto &s : end_s_candidates)
                {
                    for (const auto &d : end_d_candidates)
                    {
                        State end_d_state = {d, 0.0, 0.0};
                        end_d_conditions.emplace_back(end_d_state, s);
                    }
                }
            }
            else
            {
                std::array<double, 2> end_s_candidates = {8.0, 14.0};
                std::array<double, 3> end_d_candidates = {
                    lane_center_l,
                    lane_width * 0.5,
                    lane_width,
                };
                for (const auto &s : end_s_candidates)
                {
                    for (const auto &d : end_d_candidates)
                    {
                        State end_d_state = {d, 0.0, 0.0};
                        end_d_conditions.emplace_back(end_d_state, s);
                    }
                }
            }
        }
        else if (ReferenceLineInfo::lane_change_state == READY_TO_CHANGE_LEFT)
        {
            std::array<double, 2> end_s_candidates = {8.0, 12.0};
            ADEBUG << "left end_seed_width: " << lane_width;
            State end_d_state = {lane_width, 0.0, 0.0};
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[0]);
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[1]);
        }
        else if (ReferenceLineInfo::lane_change_state == READY_TO_CHANGE_RIGHT)
        {
            std::array<double, 2> end_s_candidates = {14.0, 24.0};
            ADEBUG << "right end_seed_width: " << -lane_width;
            State end_d_state = {-lane_width, 0.0, 0.0};
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[0]);
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[1]);
        }
        else if (ReferenceLineInfo::lane_change_state == READY_TO_CHANGE_BACK_FROM_LEFT ||
                 ReferenceLineInfo::lane_change_state == ENFORCE_CHANGE_BACK_FROM_LEFT ||
                 ReferenceLineInfo::lane_change_state == READY_TO_CHANGE_BACK_FROM_RIGHT ||
                 ReferenceLineInfo::lane_change_state == ENFORCE_CHANGE_BACK_FROM_RIGHT)
        {
            std::array<double, 2> end_s_candidates = {14.0, 24.0};
            State end_d_state = {0.0, 0.0, 0.0};
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[0]);
            end_d_conditions.emplace_back(end_d_state, end_s_candidates[1]);
        }
        else
        {
            if (ReferenceLineInfo::lane_change_state == CHANGE_LEFT_SUCCESS)
            {
                lane_center_l = lane_width;
                lane_left_l = lane_width + seed_l;
                lane_right_l = lane_width - seed_l;
            }
            else if (ReferenceLineInfo::lane_change_state == CHANGE_RIGHT_SUCCESS)
            {
                lane_center_l = -lane_width;
                lane_left_l = -lane_width + seed_l;
                lane_right_l = -lane_width - seed_l;
            }

            ADEBUG << "lane_center_l: " << lane_center_l
                   << " ;lane_left_l: " << lane_left_l
                   << " ;lane_right_l: " << lane_right_l;

            std::array<double, 4> end_s_candidates = {5.0, 12.0, 27.0, 45.0};
            std::array<double, 3> end_d_candidates =
                {lane_center_l, lane_left_l, lane_right_l};
            for (const auto &s : end_s_candidates)
            {
                for (const auto &d : end_d_candidates)
                {
                    State end_d_state = {d, 0.0, 0.0};
                    end_d_conditions.emplace_back(end_d_state, s);
                }
            }
        }
        return end_d_conditions;
    }
    // for every time_sample point, sample full range velocity
    std::vector<Condition> EndConditionSampler::SampleLonEndConditionsForCruising(
        const double ref_cruise_speed) const
    {
        // time interval is one second plus the last one 0.01
        // constexpr std::size_t num_of_time_samples = ;
        std::vector<double> time_samples;
        time_samples.clear();
        time_samples.resize(FLAGS_trajectory_time_length);
        for (std::size_t i = 0; i < FLAGS_trajectory_time_length; ++i)
        {
            time_samples[i] = FLAGS_trajectory_time_length - i;
        }

        std::vector<Condition> end_s_conditions;
        for (const auto &time : time_samples)
        {
            double v_upper =
                std::min(feasible_region_.VUpper(time), ref_cruise_speed);
            double v_lower = feasible_region_.VLower(time);

            State lower_end_s = {0.0, v_lower, 0.0};
            end_s_conditions.emplace_back(lower_end_s, time);

            State upper_end_s = {0.0, v_upper, 0.0};
            end_s_conditions.emplace_back(upper_end_s, time);

            double v_range = v_upper - v_lower;
            // Number of sample velocities
            std::size_t num_of_mid_points = std::min(
                static_cast<std::size_t>(FLAGS_num_velocity_sample - 2),
                static_cast<std::size_t>(v_range / FLAGS_min_velocity_sample_gap));

            if (num_of_mid_points > 0)
            {
                double velocity_seg = v_range / (num_of_mid_points + 1);
                for (std::size_t i = 1; i <= num_of_mid_points; ++i)
                {
                    State end_s = {0.0, v_lower + velocity_seg * i, 0.0};
                    end_s_conditions.emplace_back(end_s, time);
                }
            }
        }
        return end_s_conditions;
    }

    std::vector<Condition> EndConditionSampler::SampleLonEndConditionsForStopping(
        const double ref_stop_point) const
    {
        // time interval is one second plus the last one 0.01
        constexpr std::size_t num_time_section = 5;
        std::array<double, num_time_section> time_sections;
        for (std::size_t i = 0; i + 1 < num_time_section; ++i)
        {
            time_sections[i] = FLAGS_trajectory_time_length - i;
        }
        time_sections[num_time_section - 1] = FLAGS_polynomial_minimal_param;

        std::vector<Condition> end_s_conditions;
        for (const auto &time : time_sections)
        {
            // State end_s = {std::max(init_s_[0], ref_stop_point), 0.0, 0.0};
            State end_s = {ref_stop_point, 0.0, 0.0};
            end_s_conditions.emplace_back(end_s, time);
        }
        return end_s_conditions;
    }

    std::vector<Condition> EndConditionSampler::SampleLonEndConditionsForPathTimePoints() const
    {
        std::vector<Condition> end_s_conditions;

        std::vector<SamplePoint> sample_points = QueryPathTimeObstacleSamplePoints();

        for (const SamplePoint &sample_point : sample_points)
        {
            if (sample_point.path_time_point.t < FLAGS_polynomial_minimal_param)
            {
                continue;
            }
            double s = sample_point.path_time_point.s;
            double v = sample_point.ref_v;
            double t = sample_point.path_time_point.t;
            if (s > feasible_region_.SUpper(t) || s < feasible_region_.SLower(t))
            {
                continue;
            }
            State end_state = {s, v, 0.0};
            end_s_conditions.emplace_back(end_state, t);
        }
        return end_s_conditions;
    }

    std::vector<SamplePoint> EndConditionSampler::QueryPathTimeObstacleSamplePoints() const
    {
        std::vector<SamplePoint> sample_points;

        for (const auto &path_time_obstacle : ptr_path_time_graph_->GetPathTimeObstacles())
        {
            std::string obstacle_id = path_time_obstacle.obstacle_id;
            QueryFollowPathTimePoints(obstacle_id, &sample_points);
            ADEBUG << "PathTimeObstacles id = " << obstacle_id
                   << " follow PathTimePoints num = " << sample_points.size();
            unsigned int fptp_num = sample_points.size();
            QueryOvertakePathTimePoints(obstacle_id, &sample_points);
            ADEBUG << "Overtake PathTimePoints num = " << sample_points.size() - fptp_num;
        }
        return sample_points;
    }

    void EndConditionSampler::QueryFollowPathTimePoints(
        const std::string &obstacle_id,
        std::vector<SamplePoint> *const sample_points) const
    {
        std::vector<PathTimePoint> follow_path_time_points =
            ptr_path_time_graph_->GetObstacleSurroundingPoints(
                obstacle_id, -FLAGS_lattice_epsilon, FLAGS_time_min_density);
        for (const auto &path_time_point : follow_path_time_points)
        {
            double v = ptr_prediction_querier_->ProjectVelocityAlongReferenceLine(
                obstacle_id, path_time_point.s, path_time_point.t);
            // Generate candidate s
            double s_upper = path_time_point.s - car_params.length;
            double s_lower = s_upper - FLAGS_default_lon_buffer;
            double s_gap =
                FLAGS_default_lon_buffer / static_cast<double>(FLAGS_num_sample_follow_per_timestamp - 1);
            for (std::size_t i = 0; i < FLAGS_num_sample_follow_per_timestamp; ++i)
            {
                double s = s_lower + s_gap * static_cast<double>(i);
                SamplePoint sample_point;

                sample_point.path_time_point = path_time_point;
                sample_point.path_time_point.s = s;
                sample_point.ref_v = v;
                sample_points->push_back(std::move(sample_point));
            }
        }
    }

    void EndConditionSampler::QueryOvertakePathTimePoints(
        const std::string &obstacle_id,
        std::vector<SamplePoint> *sample_points) const
    {
        std::vector<PathTimePoint> overtake_path_time_points =
            ptr_path_time_graph_->GetObstacleSurroundingPoints(
                obstacle_id, FLAGS_lattice_epsilon, FLAGS_time_min_density);
        for (const auto &path_time_point : overtake_path_time_points)
        {
            double v = ptr_prediction_querier_->ProjectVelocityAlongReferenceLine(
                obstacle_id, path_time_point.s, path_time_point.t);

            SamplePoint sample_point;
            sample_point.path_time_point = path_time_point;
            sample_point.path_time_point.s = path_time_point.s + FLAGS_default_lon_buffer;
            sample_point.ref_v = v;
            sample_points->push_back(std::move(sample_point));
        }
    }
} // namespace PlannerHNS
