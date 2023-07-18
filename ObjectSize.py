import cv2
import pyrealsense2 as rs
from detectron2.engine import DefaultPredictor
from detectron2.config import get_cfg
import numpy as np

# Loading Aruco Marker
parameters = cv2.aruco.DetectorParameters()
aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_5X5_50)

# Configure RealSense pipeline
pipeline = rs.pipeline()
config = rs.config()
config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
pipeline.start(config)

# Load Detectron2 config and model weights
cfg = get_cfg()
cfg.merge_from_file("path/to/config.yaml")
cfg.MODEL.ROI_HEADS.SCORE_THRESH_TEST = 0.5
cfg.MODEL.WEIGHTS = "path/to/model.pth"
predictor = DefaultPredictor(cfg)

try:
    while True:
        # Wait for a new frame from the camera
        frames = pipeline.wait_for_frames()
        color_frame = frames.get_color_frame()
        if not color_frame:
            continue

        # Convert the frame to OpenCV format
        pic = np.asanyarray(color_frame.get_data())

        # Detect objects using Detectron2
        outputs = predictor(pic)

        # Retrieve the bounding box predictions
        boxes = outputs["instances"].pred_boxes.tensor.cpu().numpy()

        # Display the bounding boxes
        for box in boxes:
            x1, y1, x2, y2 = box
            cv2.rectangle(pic, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 0), 2)

        # Detect Aruco markers
        corners, _, _ = cv2.aruco.detectMarkers(pic, aruco_dict, parameters=parameters)

        if corners:
            int_corners = np.int0(corners)
            cv2.polylines(pic, int_corners, True, (0, 255, 0), 5)

        # Display the image
        cv2.imshow("Image", pic)

        # Check for key press
        key = cv2.waitKey(1)
        if key == ord('q'):
            break

finally:
    # Stop the RealSense pipeline
    pipeline.stop()

# Close all windows
cv2.destroyAllWindows()