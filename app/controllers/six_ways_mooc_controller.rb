class SixWaysMoocController < ApplicationController
  before_action :lock_under_feature_flag
  before_action :authorize_student, except: :index

  helper_method :current_mooc_student

  # GET /sixways - the landing page for sixways
  def index
  end

  # GET /sixways/start - the start page for the course
  def start
    redirect_to sixways_student_details_path unless current_mooc_student.details_complete?
  end

  # GET /sixways/student_details - page to collect basic info of the student
  def student_details
  end

  # POST /sixways/save_student_details - save the details received and redirect to start of course
  def save_student_details
    if @current_mooc_student.update(update_params)
      flash[:success] = 'Your details have been saved!'
      redirect_to sixways_start_path
    else
      render 'student_details'
    end
  end

  # GET /sixways/chapter/:id/:section_id - displays the content of a chapter's section
  def chapter
    raise_not_found unless section_exists?

    render "six_ways_mooc/chapters/chapter_#{params[:id]}_#{params[:section_id]}"
  end

  protected

  def current_mooc_student
    @current_mooc_student ||= begin
      return nil unless current_user.present?
      MoocStudent.where(user: current_user).first_or_create! skip_validation: true
    end
  end

  private

  def lock_under_feature_flag
    raise_not_found unless feature_active? :six_ways_mooc
  end

  def authorize_student
    request_authentication if current_mooc_student.blank?
  end

  def request_authentication
    redirect_to user_sessions_new_path(token: params[:token], referer: request.url)
  end

  def update_params
    params.require(:mooc_student).permit(:name, :gender, :university_id, :college, :semester, :state)
  end

  # TODO: is there a way to avoid updating these arrays manually ?
  # check if given section exists for the given chapter
  def section_exists?
    case params[:id].to_i
      when 1
        params[:section_id].to_i.in? [1, 2]
      when 2
        params[:section_id].to_i.in? [1]
      else
        false
    end
  end
end
