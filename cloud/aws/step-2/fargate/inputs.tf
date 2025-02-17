module "env" {
  source = "../env"
}

variable "region_name" {}

variable "name" {}

variable "environment" {
  type = list(object({
    name  = string
    value = string
  }))
}

variable "vpc_id" {}

variable "subnet_id" {}

variable "task_cpu" {}

variable "task_memory" {}

variable "ecs_cluster_id" {}

variable "ecs_cluster_name" {}

variable "ecs_task_execution_role_arn" {}

variable "docker_image" {}

variable "ingress_subnet_cidrs" {}

variable "entrypoint" {
  type = string
}

variable "port" {}

variable "storage_type" {
  default = "ATTACHED"

  validation {
    condition     = contains(["EFS", "ATTACHED"], var.storage_type)
    error_message = "Allowed values for vast_server_storage are \"EFS\" or \"ATTACHED\"."
  }
}

variable "storage_mount_point" {
  description = "The path of the storage volume within the container."
}
