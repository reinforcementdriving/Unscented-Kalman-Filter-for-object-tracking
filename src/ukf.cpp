#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.5; //30;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.15; //30;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  // Predicted Sigma Points
  //Xsig_pred_ = MatrixXd::Zero(n_x_, 2*n_aug_+1);
  // Last time stamp
  time_us_ = 0.0;
  // Flag variable for initialization
  is_initialized_ = false;
   ///* State dimension
  n_x_ = 5;

  ///* Augmented state dimension
  n_aug_ = 7;

  ///* predicted sigma points matrix
  //Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  ///* Weights of sigma points
  weights_ = VectorXd::Zero(2*n_aug_+1);
 
  ///* Sigma point spreading parameter
  lambda_ = 3 - n_aug_;
  NIS_L_ = 0.0;
  NIS_R_ = 0.0;
}

/**
* Destructor.
*/
UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if (!is_initialized_)
  {
    //first measurement
    x_ << 0, 0, 0, 0, 0;

    // covariance matrix
    P_ << 1, 0, 0, 0, 0,
          0, 1, 0, 0, 0,
          0, 0, 10,0, 0,
          0, 0, 0,10, 0,
          0, 0, 0, 0,10;

    //Initialize state with first measurements
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
      // Retreive raw values
      float rho = meas_package.raw_measurements_(0);
      float theta = meas_package.raw_measurements_(1);
      float ro_dot = meas_package.raw_measurements_(2);
      // conversion from polar to cartesian coordinate system
      x_[0] = rho * cos(theta);
      x_[1] = rho * sin(theta);
      x_[2] = ro_dot;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
      // map raw values directly, already cartesian
      x_[0] = meas_package.raw_measurements_(0); //px
      x_[1] = meas_package.raw_measurements_(1); //py
    }

    //compute the time elapsed between current and previous measurements
    float delta_t = (meas_package.timestamp_ - time_us_)/ 1000000.0;
    time_us_ = meas_package.timestamp_;

    is_initialized_ = true;
    std::cout<<"Initialized.."<<std::endl;
    return;

  }
  
  //compute the time elapsed between current and previous measurements
  float delta_t = (meas_package.timestamp_ - time_us_)/ 1000000.0;
  
  //Predict
  Prediction(delta_t);
  //check sensor type for update
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR and use_radar_)
  {
    UpdateRadar(meas_package);
  }
  if (meas_package.sensor_type_ == MeasurementPackage::LASER and use_laser_)
  {
    UpdateLidar(meas_package);
  }
  // update last timestep with current time
  time_us_ = meas_package.timestamp_;
  
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  cout<<"Begin Prediction"<<std::endl;
  // SIGMA POINT GENERATION
  // Create augmented mean vector 
  VectorXd x_aug = VectorXd(n_aug_); // 7
  // Create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);// 7x7
  // Create augmented sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2*n_aug_+1); // 7x14

  // Update augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  P_aug.fill(0.0);
  // Update augmented covaraince matrix
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  // Update augmented sigma points
  Xsig_aug.col(0) = x_aug;

  MatrixXd sqrt_P_aug = P_aug.llt().matrixL();

  for(int i=0; i<n_aug_; i++)
  {
    Xsig_aug.col(i+1) = x_aug + sqrt(lambda_+n_aug_) * sqrt_P_aug.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * sqrt_P_aug.col(i);
  }

  // SIGMA POINT PREDICTION
  // Create matrix for predicted sigma points
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1) ; // 5x5
  for (int i=0; i<2*n_aug_+1; i++)
  {
      // Retreive data from augmented sigma points
      double px = Xsig_aug(0,i);
      double py = Xsig_aug(1,i);
      double v =  Xsig_aug(2,i);
      double yaw = Xsig_aug(3,i);
      double yawd = Xsig_aug(4,i);
      double nu_a = Xsig_aug(5,i);
      double nu_yawdd = Xsig_aug(6,i);
      // Predicted states
      double px_p, py_p, v_p, yaw_p, yawd_p;
      // Update predicted states 
      // Avoid division by zero
      if (fabs(yawd) > 0.001)
      {
        px_p = px + v/yawd * (sin(yaw + yawd*delta_t) - sin(yaw));
        py_p = py + v/yawd * (cos(yaw) - cos(yaw+yawd*delta_t));
      }
      else
      {
        px_p = px + v * delta_t * cos(yaw);
        py_p = py + v * delta_t * sin(yaw);
      }

      v_p = v;
      yaw_p = yaw + delta_t * yawd;
      yawd_p = yawd;

      // Adding noise
      px_p = px_p + 0.5 * delta_t * delta_t * cos(yaw) * nu_a;
      py_p = py_p + 0.5 * delta_t * delta_t * sin(yaw) * nu_a;
      v_p = v_p + delta_t * nu_a;

      yaw_p = yaw_p + 0.5 * delta_t * delta_t * nu_yawdd;
      yawd_p = yawd_p + delta_t * nu_yawdd;

      // Assign predicted sigma points 
      Xsig_pred_(0, i) = px_p;
      Xsig_pred_(1, i) = py_p;
      Xsig_pred_(2, i) = v_p;
      Xsig_pred_(3, i) = yaw_p;
      Xsig_pred_(4, i) = yawd_p;
  }

  // CALCULATE PREDICTED MEAN AND COVARIANCE
  // set weights
  double weight_0 = lambda_ / (lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i =1; i<2*n_aug_+1; i++)
  {
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }

  //predicted state mean
  x_.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  P_.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalize
    while(x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();

  }
  //std::cout<<"Prediction Complete.."<<std::endl;
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  std::cout<<"Begin Processing Lidar.."<<std::endl;
  // Measurement matrix
  MatrixXd R_laser_ = MatrixXd(2,2);

  R_laser_ << std_laspx_*std_laspx_, 0,
        0, std_laspy_*std_laspy_;

  MatrixXd H_laser_ = MatrixXd(2,5);
  H_laser_ << 1,0,0,0,0,
              0,1,0,0,0;
  // Incoming measurement
  VectorXd z = VectorXd(2);
  z(0) = meas_package.raw_measurements_[0];
  z(1) = meas_package.raw_measurements_[1];

  // Relating prediction to measurement
  VectorXd z_pred = H_laser_ * x_;
  // Finding the difference
  VectorXd y = z - z_pred;
  // Mapping error matrix
  MatrixXd Ht = H_laser_.transpose();
  MatrixXd S = H_laser_ * P_ * Ht + R_laser_;
  // Updating Kalman Gain
  MatrixXd Si = S.inverse();
  MatrixXd PHt = P_ * Ht;
  MatrixXd K = PHt * Si;

  // New State Estimates
  x_ = x_ + (K * y);
  long x_size = x_.size();
  MatrixXd I = MatrixXd::Identity(x_size, x_size);
  P_ = (I - K * H_laser_)*P_;

  NIS_L_ = y.transpose() * S.inverse() * y;
  
  //std::cout<<"NAN"<<NIS_L_<<y<<std::endl;

  return;
}


/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  // Measurement dimensions, radar:(r, phi, r_dot)
  std::cout<<"Begin Processing Radar.."<<std::endl;
  int n_z_ = 3;
  // Matrix of sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z_, 2*n_aug_+1); // 3x15
  // Predicted measurement coavariance
  MatrixXd S = MatrixXd(n_z_, n_z_); // 3x3
  S.fill(0.0);
  S(0,0) = std_radr_*std_radr_;
  S(1,1) = std_radphi_*std_radphi_;
  S(2,2) = std_radrd_*std_radrd_;
  // Incoming measurement
  VectorXd z = VectorXd(n_z_); // 3
  // Mean predicted mean measurement
  VectorXd z_pred = VectorXd(n_z_);
  z_pred.fill(0.0);
  // Cross-Correlation Matrix
  MatrixXd Tc = MatrixXd(n_x_, n_z_); // 5x3
  
  // Transform sigma points into measurement space
  for (int i=0; i<2*n_aug_+1; i++)
  {
    
    double px = Xsig_pred_(0,i);
    double py = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double yawd = Xsig_pred_(4,i);
    // radar measurements  
    double rho = sqrt(px*px+py*py);
    double psi = atan2(py,px);
    double vx = px*cos(yaw)*v;
    double vy = py*sin(yaw)*v;
    double ro_dot = (vx + vy)/rho;
    
    // Check for division by zero
    if (fabs(rho) < 0.0001)
    {
      cout<<"Division by zero error"<<endl;
      cin>>rho;
    }

    Zsig(0,i) = rho;
    Zsig(1,i) = psi;
    Zsig(2,i) = ro_dot;
    
    //predicted measurement mean 
    z_pred += weights_(i) * Zsig.col(i);
  }
  
  // Update predicted measurement covariance
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    while(z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
    S += weights_(i)*z_diff*z_diff.transpose();
  }
  
  // Calculate cross correlation matrix
  Tc.fill(0.0);
  for(int i=0; i<2 * n_aug_ +1; i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    while(x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

    VectorXd z_diff = Zsig.col(i) - z_pred;
    while(z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  
  // Kalman gain K
  MatrixXd K = Tc * S.inverse();

  // Update z with measurement values
  z(0) = meas_package.raw_measurements_(0);
  z(1) = meas_package.raw_measurements_(1);
  z(2) = meas_package.raw_measurements_(2);

  VectorXd z_diff = z - z_pred;
  while(z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
  while(z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;

  P_ = P_ - K*S*K.transpose();

  NIS_R_ = z_diff.transpose() * S.inverse() * z_diff;
  
  
  //std::cout<<"NAN"<<NIS_R_<<z_diff<<std::endl;
	
  //std::cout<<"Processed Radar.."<<std::endl;

  return;

}
